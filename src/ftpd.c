 /*
 * cbmdisk - network enabled, sd card based IEEE-488 CBM floppy emulator 
 * Copyright (C) 2015 Guenter Bartsch
 * 
 * Most of the code originates from:
 *
 * NODISKEMU - SD/MMC to IEEE-488 interface/controller
 * Copyright (c) 2015 Nils Eilers. 
 *
 * which is based on:
 *
 * sd2iec by Ingo Korb (et al.), http://sd2iec.de
 * Copyright (C) 2007-2014  Ingo Korb <ingo@akana.de>
 *
 * Inspired by MMC2IEC by Lars Pontoppidan et al.
 * FAT filesystem access based on code from ChaN and Jim Brain, see ff.c|h.
 *
 * Network code is based on ETH_M32_EX 
 * Copyright (C) 2007 by Radig Ulrich <mail@ulrichradig.de>
 *
 * JiffyDos send based on code by M.Kiesel
 * Fat LFN support and lots of other ideas+code by Jim Brain 
 * Final Cartridge III fastloader support by Thomas Giesel 
 * Original IEEE488 support by Nils Eilers 
 * FTP server and most of the IEEE 488 FSM implementation by G. Bartsch.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License only.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *
 *
 * Tiny FTP Server Implementation, based on CBM DOS fileops interface
 *
 */

#include "config.h"
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "tcpip.h"
#include "timer.h"
#include "uart.h"

#include "ftpd.h"

#include "buffers.h"
#include "doscmd.h"
#include "fileops.h"
#include "utils.h"

#define DEBUG_FTPD 


#ifdef DEBUG_FTPD
# define DEBUG_PUTS_P(x) uart_puts_P(PSTR(x))
# define DEBUG_PUTS(x) uart_puts(x)
# define DEBUG_PUTHEX(x) uart_puthex(x)
# define DEBUG_PUTC(x)   uart_putc(x)
# define DEBUG_FLUSH()   uart_flush()
# define DEBUG_PUTCRLF() uart_putcrlf()
# define DEBUG_PRINTF(...) printf(__VA_ARGS__)
#else
# define DEBUG_PUTS_P(x) do {} while (0)
# define DEBUG_PUTS(x) do {} while (0)
# define DEBUG_PUTHEX(x) do {} while (0)
# define DEBUG_PUTC(x) do {} while (0)
# define DEBUG_FLUSH() do {} while (0)
# define DEBUG_PUTCRLF() do {} while (0)
# define DEBUG_PRINTF(...) while(0)
#endif

#define FTPD_PORT                    21
#define DATA_PORT_RANGE_START        2048

#define MAX_CMD_BUF 255
#define FTP_SA      7

#define STATE_START	     	 1
#define STATE_LIST_START   2
#define STATE_LIST_FIN     4

#define STATE_STOR        10
#define STATE_STOR_FIN    11
#define STATE_STOR_COMP   12

#define STATE_RETR_START  20
#define STATE_RETR_FIN    21

typedef struct {

	uint8_t      state;
	unsigned int data_port;
	uint8_t      data_idx;

} ftp_table_t;

static unsigned int next_data_port = DATA_PORT_RANGE_START;
static ftp_table_t ftp_entry[MAX_TCP_ENTRY];
static uint8_t zero = 0;

#define printf_packet(index, str,...) printf_packet_P(index, PSTR(str), ##__VA_ARGS__)

static char cbm_petscii2ascii_c(char ch) {

	uint8_t uch = (uint8_t) ch;

	if (uch > 64 && uch < 91)
		uch += 32;
	else if (uch > 96 && uch < 123)
		uch -= 32;
	else if (uch > 192 && uch < 219)
		uch -= 128;
	else if (uch == 0x0d)
		uch = 0x0a;
	else if ( (uch == 0xa0) || (uch == 0xe0) )
		uch = 0x0a;

	ch = (char) uch;

	return isprint(ch) ? ch : 0;
}

static void printf_packet_P (unsigned char index, const char *fmt, ...) {

	va_list ap;
	int n;
  char *buf;

	va_start(ap, fmt);

	buf = (char *) &eth_buffer[TCP_DATA_START];
	n = vsnprintf_P (buf, 256, fmt, ap);

	DEBUG_PUTS(buf);

	create_new_tcp_packet(n, index);
	tcp_entry[index].time = TCP_TIME_OFF;
	tcp_entry[index].status = ACK_FLAG | PSH_FLAG;

	va_end(ap);
}


static uint8_t dir_send_packet (uint8_t idx) {

	buffer_t *buf;
	int n = 0;

	while (1) {

		uint8_t packet_generated = 0;

		buf = find_buffer(FTP_SA);

		if (!buf) {
			//printf ("T0\r\n");
			return 0;
		} 

		char pc = buf->data[buf->position];
		char c  = cbm_petscii2ascii_c(pc);

		if (c == 0) {
			if (!zero) {
				DEBUG_PUTCRLF();
				//printf_packet(idx, "\r\n");

				eth_buffer[TCP_DATA_START + (n++)] = '\r';
				eth_buffer[TCP_DATA_START + (n++)] = '\n';

				packet_generated = 1;
				zero = 1;
			}
		} else {
			zero = 0;
			eth_buffer[TCP_DATA_START + (n++)] = c;

			//printf_packet(idx, "%c", c);
			//packet_generated = 1;
			DEBUG_PUTC(c);
		}

		//uart_puthex (buf->lastused);
		//DEBUG_PUTCRLF();
		//DEBUG_FLUSH();

		if ((buf->position == buf->lastused) && buf->sendeoi) {
			//DEBUG_PRINTF ("T1\r\n");
			return 0;
		}

		if (buf->position == buf->lastused) {
			if (buf->sendeoi && FTP_SA != 15 && !buf->recordlen &&
					buf->refill != directbuffer_refill) {
				buf->read = 0;
				//DEBUG_PRINTF ("T2\r\n");
				return 0;
			} 
			if (buf->refill(buf)) {             // Refill buffer
				//DEBUG_PRINTF ("T3\r\n");
				return 0;
			}
			// Search the buffer again, it can change when using large buffers
			buf = find_buffer(FTP_SA);
			//DEBUG_PRINTF ("G0\r\n");
		} else {
			buf->position++;
			//DEBUG_PRINTF ("G1\r\n");
		}

		if (packet_generated) {
			break;
		}
	}
	
	create_new_tcp_packet(n, idx);
	tcp_entry[idx].time = TCP_TIME_OFF;

	return 1;
}

static uint8_t retr_send_packet (uint8_t idx) {

	buffer_t *buf;
	int n = 0;

	buf = find_buffer(FTP_SA);

	if (!buf) {
		return 0;
	} 

	DEBUG_PUTS_P("RETR DATA DUMP:\r\n");
	DEBUG_FLUSH();
	while (buf->position < buf->lastused) {
		char pc = buf->data[buf->position++];
		eth_buffer[TCP_DATA_START + (n++)] = pc;
		DEBUG_PUTC(pc);
		if ( (buf->position % 16) == 0)
			DEBUG_FLUSH();
	}
	DEBUG_PUTS_P("\r\nRETR DATA DUMP ENDS\r\n");
	DEBUG_FLUSH();

	create_new_tcp_packet(n, idx);
	tcp_entry[idx].time = TCP_TIME_OFF;

	if (buf->sendeoi) {
		return 0;
	}

	if (buf->sendeoi && FTP_SA != 15 && !buf->recordlen &&
			buf->refill != directbuffer_refill) {
		buf->read = 0;
		return 0;
	} 
	if (buf->refill(buf)) {             // Refill buffer
		return 0;
	}

	return 1;
}

static uint8_t stor_byte (uint8_t c) {
	buffer_t *buf;

	buf = find_buffer(FTP_SA);

	if (!buf) {
		return 0;
	} 

	// Flush buffer if full
	if (buf->mustflush) {
	  DEBUG_PUTS_P("MUSTFLUSH\r\n");
		if (buf->refill(buf)) {
			DEBUG_PUTS_P("REFILL ABORT\r\n");
			return 0;
		}
		// Search the buffer again,
		// it can change when using large buffers
		buf = find_buffer(FTP_SA);
	}

	buf->data[buf->position] = c;
	mark_buffer_dirty(buf);

	if (buf->lastused < buf->position) buf->lastused = buf->position;
	buf->position++;

	// Mark buffer for flushing if position wrapped
	if (buf->position == 0) buf->mustflush = 1;

}

static void stor_fin (void) {
	buffer_t *buf;

	buf = find_buffer(FTP_SA);

	if (!buf) {
		return 0;
	} 

	buf->cleanup(buf);
	free_buffer(buf);

}


static void ftp_data (unsigned char index)
{

	DEBUG_PRINTF ("\r\nidx=%02x app_status=%04x FTPDATA\r\n", index, tcp_entry[index].app_status);
	DEBUG_FLUSH();

	// find corresponding ctrl connection
	
	ftp_table_t *self = NULL;
  uint8_t ctrl_index = 0;
	unsigned int dest_port = ntohs(tcp_entry[index].dest_port);
	for (ctrl_index = 0; ctrl_index<MAX_TCP_ENTRY; ctrl_index++) {
		if (ftp_entry[ctrl_index].data_port == dest_port) {
			self = &ftp_entry[ctrl_index];
			break;
		}
	}

	if (!self) {
		printf ("*** ERROR: FTPDATA failed to find ctrl connection for data port %d!\r\n", dest_port);
		return;
	}

	DEBUG_PRINTF("   ctrl connection found, state: %d\n\r", self->state);
	self->data_idx = index;

	if (tcp_entry[index].status & FIN_FLAG)
	{
	  DEBUG_PRINTF("   DATA FIN\n\r");

		if (self->state == STATE_STOR) {
			self->state = STATE_STOR_COMP;
			stor_fin();
		} else if (self->state == STATE_STOR_FIN) {
			printf_packet (ctrl_index, "226 Transfer complete.\r\n");
			self->state = STATE_START;
			stor_fin();
		}

		return;
	}

	if (tcp_entry[index].app_status < 0xFFFE)
	{

		tcp_entry[index].app_status = 1; 
		tcp_entry[index].status = ACK_FLAG;

	  switch (self->state) {
			case STATE_LIST_START:
				if (!dir_send_packet(index)) {
		
					buffer_t *buf = find_buffer(FTP_SA);
					buf->cleanup(buf);
					free_buffer(buf);

					printf_packet (index, "\r\n");
					self->state = STATE_LIST_FIN;
				}
				break;
			case STATE_LIST_FIN:
				self->state = STATE_START;
				DEBUG_PUTS_P("FTPDATA CLOSE\n\r");
				tcp_entry[index].app_status = 0xFFFE;
				tcp_Port_close(index);
				printf_packet (ctrl_index, "226 Transfer complete.\r\n");
				break;

			case STATE_STOR:
				//DEBUG_PUTS_P("STOR DATA DUMP:\r\n");
				//DEBUG_FLUSH();
				for (int a = TCP_DATA_START_VAR; a<(TCP_DATA_END_VAR+1); a++)
				{
					uint8_t receive_char = eth_buffer[a];
					stor_byte (receive_char);
					//DEBUG_PUTC(receive_char);
					//DEBUG_PUTC('[');
					//DEBUG_PUTHEX(receive_char);
					//DEBUG_PUTC(']');
					//if ( (a % 16) == 0)
					//	DEBUG_FLUSH();
				}	
				//DEBUG_PUTS_P("\r\nSTOR DATA DUMP ENDS\r\n");
				//DEBUG_FLUSH();
				
				create_new_tcp_packet(0,index);
				break;

			case STATE_RETR_START:
				if (!retr_send_packet(index)) {
		
					buffer_t *buf = find_buffer(FTP_SA);
					buf->cleanup(buf);
					free_buffer(buf);

					create_new_tcp_packet(0,index);
					self->state = STATE_RETR_FIN;
				}
				break;

			case STATE_RETR_FIN:
				self->state = STATE_START;
				DEBUG_PUTS_P("FTPDATA CLOSE\n\r");
				tcp_entry[index].app_status = 0xFFFE;
				tcp_Port_close(index);
				printf_packet (ctrl_index, "226 Transfer complete.\r\n");
				break;

		}	

		return;
	}	

	return;
}

static void ftp_ctrl (unsigned char index)
{

  char    cmd_buf[MAX_CMD_BUF];
	uint8_t cmd_len = 0;

	ftp_table_t *self = &ftp_entry[index];

	DEBUG_PRINTF ("\r\nidx=%02x status=%02x app_status=%04x state=%02x FTPCTRL\r\n", index, tcp_entry[index].status, tcp_entry[index].app_status, self->state);
	DEBUG_FLUSH();

	if (tcp_entry[index].status & FIN_FLAG)
	{
	  DEBUG_PUTS_P("   CTRL FIN\n\r");
		kill_tcp_app (self->data_port);
		return;
	}

	if (tcp_entry[index].app_status == 0 || tcp_entry[index].app_status == 1)
	{
	  DEBUG_PUTS_P("   START\n\r");
		tcp_entry[index].app_status = 1; 

		self->state     = STATE_START;
		self->data_port = next_data_port++;

		add_tcp_app (self->data_port, (void(*)(unsigned char))ftp_data);

		printf_packet (index, "220 CBM FTP READY.\r\n");

		return;
	}	

	if (tcp_entry[index].app_status > 1) {
		if (tcp_entry[index].status & PSH_FLAG) {
			tcp_entry[index].app_status = 2;	
			DEBUG_PUTC('<');

			cmd_len = 0;

			for (int a = TCP_DATA_START_VAR;a<(TCP_DATA_END_VAR+1);a++)
			{
				unsigned char receive_char;
				receive_char = eth_buffer[a];

				if (receive_char == 32 || receive_char == 10 || receive_char == 13) {
					cmd_buf[cmd_len++] = 0;
					break;
				}

				cmd_buf[cmd_len++] = receive_char;
				DEBUG_PUTC (receive_char);
			}	
			DEBUG_PUTS_P ("\n\r");
			tcp_entry[index].status =  ACK_FLAG;
			tcp_entry[index].time   = TCP_TIME_OFF;

			switch (self->state) {
				case STATE_START:

					if (!strncmp(cmd_buf, "USER", 4)) {
						printf_packet (index, "331 Password please.\r\n");
					} else if (!strncmp(cmd_buf, "PASS", 4)) {
						printf_packet (index, "230 Logged in.\r\n");
					} else if (!strncmp(cmd_buf, "SYST", 4)) {
						printf_packet (index, "215 CBM 8296\r\n");
					} else if (!strncmp(cmd_buf, "FEAT", 4)) {
						printf_packet (index, "211 END\r\n");
					} else if (!strncmp(cmd_buf, "PWD", 3)) {
						printf_packet (index, "257 \"%s\" is current directory\r\n", "/");
					} else if (!strncmp(cmd_buf, "PASV", 4)) {
						printf_packet (index, "227 Entering Passive Mode (%d,%d,%d,%d,%d,%d)\r\n", myip[0], myip[1], myip[2], myip[3], self->data_port >> 8, self->data_port % 256);
					} else if (!strncmp(cmd_buf, "ALLO", 4)) {
						// ignore
						printf_packet(index, "202 Command not implemented, superfluous at this site.\r\n");
					} else if (!strncmp(cmd_buf, "LIST", 4)) {
						printf_packet(index, "150 Opening ASCII mode data connection for list\r\n");

						self->state = STATE_LIST_START;

					} else if (!strncmp(cmd_buf, "RETR", 4)) {

						command_length = 0;
						
						for (int a = TCP_DATA_START_VAR + 5; a<(TCP_DATA_END_VAR+1); a++)
						{
							unsigned char receive_char;
							receive_char = eth_buffer[a];

							command_buffer[command_length++] = receive_char;
							if (receive_char == 32 || receive_char == 10 || receive_char == 13 || receive_char == 0) {
								break;
							}
						}	

						command_buffer[command_length] = 0;
						printf_packet(index, "150 Opening BINARY mode data connection for %s\r\n", command_buffer);

						asc2pet(command_buffer);
						self->state = STATE_RETR_START;

					} else if ((!strncmp(cmd_buf, "MKD", 3)) || (!strncmp(cmd_buf, "CWD", 3)) || (!strncmp(cmd_buf, "RMD", 3)) ) {

						command_length = 2;
						command_buffer[0] = cmd_buf[0];
						command_buffer[1] = 'D';
						
						for (int a = TCP_DATA_START_VAR + 4; a<(TCP_DATA_END_VAR+1); a++)
						{
							unsigned char receive_char;
							receive_char = eth_buffer[a];

							command_buffer[command_length++] = receive_char;
							if (receive_char == 32 || receive_char == 10 || receive_char == 13 || receive_char == 0) {
								break;
							}
						}	

						parse_doscommand();
						free_multiple_buffers(FMB_USER_CLEAN);

						printf_packet(index, "257 %s directory operation done.\r\n", command_buffer);

					} else if (!strncmp(cmd_buf, "DELE", 4)) {

						command_length = 2;
						command_buffer[0] = 's';
						command_buffer[1] = ':';
						
						for (int a = TCP_DATA_START_VAR + 5; a<(TCP_DATA_END_VAR+1); a++)
						{
							unsigned char receive_char;
							receive_char = eth_buffer[a];

							command_buffer[command_length++] = receive_char;
							if (receive_char == 32 || receive_char == 10 || receive_char == 13 || receive_char == 0) {
								break;
							}
						}	

						command_buffer[command_length] = 0;
						asc2pet(command_buffer);

						parse_doscommand();
						free_multiple_buffers(FMB_USER_CLEAN);

						printf_packet(index, "250 %s file deleted.\r\n", command_buffer);

					} else if (!strncmp(cmd_buf, "STOR", 4)) {

						command_length = 0;
						
						for (int a = TCP_DATA_START_VAR + 5; a<(TCP_DATA_END_VAR+1); a++)
						{
							unsigned char receive_char;
							receive_char = eth_buffer[a];

							if (receive_char == 32 || receive_char == 10 || receive_char == 13 || receive_char == 0) {
								break;
							}
							command_buffer[command_length++] = receive_char;
						}	
						command_buffer[command_length++] = ',';
						command_buffer[command_length++] = 'p';
						command_buffer[command_length++] = ',';
						command_buffer[command_length++] = 'w';

						command_buffer[command_length] = 0;
						asc2pet(command_buffer);

						for (uint8_t i = 0; i<command_length; i++)
							DEBUG_PUTC (command_buffer[i]);
						DEBUG_PUTCRLF();
						DEBUG_FLUSH();

						file_open(FTP_SA);

						self->state = STATE_STOR;
						printf_packet(index, "150 READY.\r\n");

					} else {
						printf("   %s UNK", cmd_buf);
						printf_packet(index, "502 command not implemented.\r\n");
					}
					break;
				default:
					printf("   ERROR - unimplemented state %d", self->state);
					printf_packet(index, "   >421 internal error.\r\n");
					tcp_entry[index].app_status = 0xFFFE;
					tcp_Port_close(index);

			}

		} else {

			// start sending data once ACK for last packet has been received

			if (self->state == STATE_LIST_START) {

				command_buffer[0] = '$';
				command_buffer[1] = 0;
				command_length = 1;
				file_open (FTP_SA);

				// skip first "line"
				{
					buffer_t *buf;
					buf = find_buffer(FTP_SA);
					buf->refill(buf);
				}

				dir_send_packet(self->data_idx);
			
			} else if (self->state == STATE_RETR_START) {

				file_open (FTP_SA);

				retr_send_packet(self->data_idx);
				if (!retr_send_packet(self->data_idx)) {
		
					buffer_t *buf = find_buffer(FTP_SA);
					buf->cleanup(buf);
					free_buffer(buf);

					create_new_tcp_packet(0,self->data_idx);
					self->state = STATE_RETR_FIN;
				}
			} else if (self->state == STATE_STOR) {
				self->state = STATE_STOR_FIN;
			} else if (self->state == STATE_STOR_COMP) {
				printf_packet (index, "226 Transfer complete.\r\n");
				self->state = STATE_START;
			}
		}
	}


	return;
}

void ftpd_init (void)
{
	/* 
   * init static datastructures so we have a clean start after reset
   */

	next_data_port = DATA_PORT_RANGE_START;
	for (uint8_t i = 0; i<MAX_TCP_ENTRY; i++) {
		ftp_entry[i].state     = STATE_START;
		ftp_entry[i].data_port = 0;
		ftp_entry[i].data_idx  = 0;
	}
	zero = 0;

	add_tcp_app (FTPD_PORT, (void(*)(unsigned char))ftp_ctrl);
}

