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
 * Tiny FTP Server Implementation, accesses first partition through fatfs directly
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
#include "parser.h"
#include "ff.h"

//#define DEBUG_FTPD 

//#ifdef DEBUG_RETR_DATA
//#ifdef DEBUG_STORE_DATA

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

#define FTP_BUF_SIZE 255
char    ftp_buf[FTP_BUF_SIZE];

#define RETR_PKT_SIZE (MTU_SIZE - 200)

DIR     ftp_dir;

#define FTP_PART    0
static FIL ftp_file;

#define STATE_START	     	 1
#define STATE_LIST_START   2

#define STATE_STOR        10
#define STATE_STOR_FIN    11
#define STATE_STOR_COMP   12

#define STATE_RETR_START  20

typedef struct {

	uint8_t      state;
	unsigned int data_port;
	uint8_t      data_idx;

} ftp_table_t;

static unsigned int next_data_port = DATA_PORT_RANGE_START;
static ftp_table_t ftp_entry[MAX_TCP_ENTRY];

#define printf_packet(index, str,...) printf_packet_P(index, PSTR(str), ##__VA_ARGS__)

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

static void get_filename (uint8_t offset) {

	command_length = 0;
	
	for (int a = TCP_DATA_START_VAR + offset; a<(TCP_DATA_END_VAR+1); a++)
	{
		unsigned char receive_char;
		receive_char = eth_buffer[a];

		if (command_length >= CONFIG_COMMAND_BUFFER_SIZE) 
			break;

		if (receive_char == 32 || receive_char == 10 || receive_char == 13 || receive_char == 0) {
			command_buffer[command_length] = 0;
			break;
		}
		command_buffer[command_length++] = receive_char;
	}	
}

static uint8_t dir_send_packet (uint8_t idx) {

	FRESULT res;
	FILINFO finf;
	int n = 0;

	tcp_entry[idx].time = TCP_TIME_OFF;

	finf.lfn = ftp_buf;

	while (1) {

		res = f_readdir (&ftp_dir, &finf);

		DEBUG_PRINTF("f_readdir res=%d\r\n", res);
		DEBUG_FLUSH();

		if (res != FR_OK) {
			//create_new_tcp_packet(0, idx); 
			return 0;
		}

		DEBUG_PRINTF("f_readdir attrib=%d, fname=%s\r\n", finf.fattrib, finf.fname);
		DEBUG_FLUSH();

		if (finf.fattrib & AM_VOL) {
			continue;
		}
		break;
	}

	snprintf (&eth_buffer[TCP_DATA_START + n], 9, "%7ld ", finf.fsize);
	n+= 8;

	for (uint8_t i = 0; i<_MAX_LFN_LENGTH; i++) {
		char ch;
		if (finf.lfn[0]) {
			ch = finf.lfn[i];
		} else {
			ch = finf.fname[i];
		}
		if (ch == 0)	
			break;

		eth_buffer[TCP_DATA_START + (n++)] = ch;
	}
	if (n == 8)
		return 0;

	eth_buffer[TCP_DATA_START + (n++)] = '\r';
	eth_buffer[TCP_DATA_START + (n++)] = '\n';

	create_new_tcp_packet(n, idx);

	return 1;
}

static uint8_t retr_send_packet (uint8_t idx) {

	uint16_t n;
	FRESULT res;

	res = f_read (&ftp_file, &eth_buffer[TCP_DATA_START], RETR_PKT_SIZE, &n);

#ifdef DEBUG_RETR_DATA
	DEBUG_PRINTF("RETR DATA DUMP (%d bytes, res=%d):\r\n", n, res);
	DEBUG_FLUSH();

	for (int i = 0; i<n; i++) {
		char pc = eth_buffer[TCP_DATA_START + i];
		DEBUG_PUTC(pc);
		DEBUG_PUTC('[');
		DEBUG_PUTHEX(pc);
		DEBUG_PUTC(']');
		DEBUG_FLUSH();
	}
	DEBUG_PUTS_P("\r\nRETR DATA DUMP ENDS\r\n");
	DEBUG_FLUSH();
#endif

	if (n>0) {
		create_new_tcp_packet(n, idx);
		tcp_entry[idx].time = TCP_TIME_OFF;
	}

	return n==0;
}

static void ftp_data (unsigned char index)
{
	uint16_t n;
	FRESULT res;

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


	if (tcp_entry[index].app_status < 0xFFFE)
	{
		switch (self->state) {
			case STATE_LIST_START:
				if (!dir_send_packet(index)) {
					self->state = STATE_START;
					DEBUG_PUTS_P("FTPDATA CLOSE\n\r");
					tcp_entry[index].app_status = 0xFFFE;
					tcp_Port_close(index);
					printf_packet (ctrl_index, "226 Transfer complete.\r\n");
				}
				break;

			case STATE_STOR:
			case STATE_STOR_FIN:

				res = f_write (&ftp_file, &eth_buffer[TCP_DATA_START_VAR], (TCP_DATA_END_VAR) - TCP_DATA_START_VAR, &n);

				DEBUG_PRINTF("STOR %d BYTES WRITTEN, res=%d.\r\n", n, res);
				DEBUG_FLUSH();

#ifdef DEBUG_STORE_DATA
				DEBUG_PUTS_P("STOR DATA DUMP:\r\n");
				DEBUG_FLUSH();
				
				for (int a = TCP_DATA_START_VAR; a<(TCP_DATA_END_VAR+1); a++)
				{
					uint8_t receive_char = eth_buffer[a];
					DEBUG_PUTC(receive_char);
					DEBUG_PUTC('[');
					DEBUG_PUTHEX(receive_char);
					DEBUG_PUTC(']');
					if ( (a % 16) == 0)
						DEBUG_FLUSH();
				}	
				DEBUG_PUTS_P("\r\nSTOR DATA DUMP ENDS\r\n");
				DEBUG_FLUSH();
#endif
				if (tcp_entry[index].status & FIN_FLAG)
				{
					DEBUG_PRINTF("   STOR DATA FIN\n\r");

					if (self->state == STATE_STOR) {
						self->state = STATE_STOR_COMP;
					} else if (self->state == STATE_STOR_FIN) {
						printf_packet (ctrl_index, "226 Transfer complete.\r\n");
						self->state = STATE_START;
					}

					f_sync (&ftp_file);
					f_close(&ftp_file);
				}

				create_new_tcp_packet(0, index);
				
				break;

			case STATE_RETR_START:
				if (retr_send_packet(index)) {
	
					f_close(&ftp_file);

					self->state = STATE_START;
					DEBUG_PUTS_P("FTPDATA CLOSE\n\r");
					tcp_entry[index].app_status = 0xFFFE;
					tcp_Port_close(index);
					printf_packet (ctrl_index, "226 Transfer complete.\r\n");
				}
				break;
		}	

		tcp_entry[index].app_status = 1; 
		tcp_entry[index].status = ACK_FLAG;
		tcp_entry[index].time   = TCP_TIME_OFF;
	}	
}

static void ftp_ctrl (unsigned char index)
{
	FRESULT res;
	uint8_t i = 0;

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

			i = 0;

			for (int a = TCP_DATA_START_VAR;a<(TCP_DATA_END_VAR+1);a++)
			{
				unsigned char receive_char;
				receive_char = eth_buffer[a];

				if (receive_char == 32 || receive_char == 10 || receive_char == 13) {
					ftp_buf[i++] = 0;
					break;
				}

				ftp_buf[i++] = receive_char;
				DEBUG_PUTC (receive_char);
			}	
			DEBUG_PUTS_P ("\n\r");
			tcp_entry[index].status =  ACK_FLAG;
			tcp_entry[index].time   = TCP_TIME_OFF;

			switch (self->state) {
				case STATE_START:

					if (!strncmp(ftp_buf, "USER", 4)) {
						printf_packet (index, "331 Password please.\r\n");
					} else if (!strncmp(ftp_buf, "PASS", 4)) {
						printf_packet (index, "230 Logged in.\r\n");
					} else if (!strncmp(ftp_buf, "SYST", 4)) {
						printf_packet (index, "215 CBM 8296\r\n");
					} else if (!strncmp(ftp_buf, "FEAT", 4)) {
						printf_packet (index, "211 END\r\n");
					} else if (!strncmp(ftp_buf, "PWD", 3)) {
						// FIXME: f_stat ?
						printf_packet (index, "257 \"%s\" is current directory\r\n", "/");
					} else if (!strncmp(ftp_buf, "PASV", 4)) {
						printf_packet (index, "227 Entering Passive Mode (%d,%d,%d,%d,%d,%d)\r\n", myip[0], myip[1], myip[2], myip[3], self->data_port >> 8, self->data_port % 256);
					} else if (!strncmp(ftp_buf, "ALLO", 4)) {
						// ignore
						printf_packet(index, "202 Command not implemented, superfluous at this site.\r\n");
					} else if (!strncmp(ftp_buf, "LIST", 4)) {
						printf_packet(index, "150 Opening ASCII mode data connection for list\r\n");

						self->state = STATE_LIST_START;

					} else if (!strncmp(ftp_buf, "RETR", 4)) {

						get_filename (5);

						res = f_open(&partition[FTP_PART].fatfs, &ftp_file, command_buffer, FA_OPEN_EXISTING | FA_READ);

						DEBUG_PRINTF("STATE_RETR_START f_open res=%d\r\n", res);
						DEBUG_FLUSH();

						if (res == FR_OK) {
							printf_packet(index, "150 Opening BINARY mode data connection for %s\r\n", command_buffer);
							self->state = STATE_RETR_START;
						} else {
							printf_packet(index, "550 File retrieval failed: %d.\r\n", res);
						}

					} else if (!strncmp(ftp_buf, "CWD", 3)) {
						get_filename (4);

						res = f_chdir (&partition[FTP_PART].fatfs, command_buffer);

						if (res == FR_OK)
							printf_packet(index, "257 current directory changed.\r\n");
						else
							printf_packet(index, "550 directory change failed: %d.\r\n", res);

					} else if (!strncmp(ftp_buf, "MKD", 3)) {

						get_filename (4);

						res = f_mkdir (&partition[FTP_PART].fatfs, command_buffer);

						if (res == FR_OK)
							printf_packet(index, "257 %s directory created.\r\n", command_buffer);
						else
							printf_packet(index, "550 directory creation failed: %d.\r\n", res);

					} else if (!strncmp(ftp_buf, "RMD", 3)) {

						get_filename (4);

						res = f_unlink (&partition[FTP_PART].fatfs, command_buffer);

						if (res == FR_OK)
							printf_packet(index, "257 %s directory deleted.\r\n", command_buffer);
						else
							printf_packet(index, "550 directory deletion failed: %d.\r\n", res);

					} else if (!strncmp(ftp_buf, "DELE", 4)) {

						get_filename (5);

						res = f_unlink (&partition[FTP_PART].fatfs, command_buffer);

						if (res == FR_OK) 
							printf_packet(index, "257 %s file deleted.\r\n", command_buffer);
						else
							printf_packet(index, "550 file deletion failed: %d.\r\n", res);

					} else if (!strncmp(ftp_buf, "STOR", 4)) {

						get_filename (5);

						res = f_open(&partition[FTP_PART].fatfs, &ftp_file, command_buffer, FA_WRITE | FA_CREATE_ALWAYS);

						if (res == FR_OK) {
							self->state = STATE_STOR;
							printf_packet(index, "150 READY.\r\n");
						} else {
							printf_packet(index, "550 store failed: %d.\r\n", res);
						}

					} else {
						printf("   %s UNK", ftp_buf);
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

				f_opendir (&partition[FTP_PART].fatfs, &ftp_dir, "");

				dir_send_packet(self->data_idx);
			
			} else if (self->state == STATE_RETR_START) {

				if (retr_send_packet(self->data_idx)) {

					f_close (&ftp_file);
		
					self->state = STATE_START;
					DEBUG_PUTS_P("FTPDATA CLOSE\n\r");
					tcp_entry[self->data_idx].app_status = 0xFFFE;
					tcp_Port_close(self->data_idx);
					printf_packet (index, "226 Transfer complete.\r\n");
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

	add_tcp_app (FTPD_PORT, (void(*)(unsigned char))ftp_ctrl);
}

