/*
 * Tiny FTP Server Implementation
 *
 * (C) 2015 by G. Bartsch <guenter@zamia.org>
 * Based on ETH_M32_EX_SOFT by Ulrich Radig <mail@ulrichradig.de>
 *
 * License: GPLv2
 */

#include "config.h"
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tcpip.h"
#include "timer.h"
#include "uart.h"

#include "ftpd.h"

#define FTPD_PORT                    21
#define DATA_PORT_RANGE_START        2048

#define MAX_CMD_BUF 255

#define STATE_START	     	1
#define STATE_LIST_START  2
#define STATE_LIST_CONT   3
#define STATE_LIST_FIN    4

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

	uart_puts(buf);

	create_new_tcp_packet(n, index);
	tcp_entry[index].time = TCP_TIME_OFF;

	va_end(ap);
}

static void ftp_data (unsigned char index)
{

	printf ("\r\nidx=%02x app_status=%04x FTPDATA\r\n", index, tcp_entry[index].app_status);
	uart_flush();

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

	printf("   ctrl connection found, state: %d\n\r", self->state);
	self->data_idx = index;

	if (tcp_entry[index].status & FIN_FLAG)
	{
	  uart_puts_P(PSTR("   FIN\n\r"));
		return;
	}

	if (tcp_entry[index].app_status < 0xFFFE)
	{

		tcp_entry[index].app_status = 1; 
		tcp_entry[index].status = ACK_FLAG;

	  switch (self->state) {
			case STATE_LIST_START:
				printf_packet (index, "foo.txt\r\n");
				self->state = STATE_LIST_CONT;
				break;
			case STATE_LIST_CONT:
				printf_packet (index, "bar.txt\r\n");
				self->state = STATE_LIST_FIN;
				break;
			case STATE_LIST_FIN:
				self->state = STATE_START;
				uart_puts_P(PSTR("FTPDATA CLOSE\n\r"));
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

	printf ("\r\nidx=%02x app_status=%04x state=%02x FTPCTRL\r\n", index, tcp_entry[index].app_status, self->state);
	uart_flush();

	if (tcp_entry[index].status & FIN_FLAG)
	{
	  uart_puts_P(PSTR("   FIN\n\r"));
		kill_tcp_app (self->data_port);
		return;
	}

	if (tcp_entry[index].app_status == 0 || tcp_entry[index].app_status == 1)
	{
	  uart_puts_P(PSTR("   START\n\r"));
		tcp_entry[index].app_status = 1; 

		self->state     = STATE_START;
		self->data_port = next_data_port++;

		add_tcp_app (self->data_port, (void(*)(unsigned char))ftp_data);

		printf_packet (index, "220 CBM FTP READY.\r\n");

		return;
	}	

	if ((tcp_entry[index].app_status > 1) && (tcp_entry[index].status&PSH_FLAG))
	{
		tcp_entry[index].app_status = 2;	
	  uart_putc('<');

	  cmd_len = 0;

		for (int a = TCP_DATA_START_VAR;a<(TCP_DATA_END_VAR+1);a++)
		{
			tcp_entry[index].time = TCP_TIME_OFF;
			unsigned char receive_char;
			receive_char = eth_buffer[a];

			if (receive_char == 32 || receive_char == 10 || receive_char == 13) {
				cmd_buf[cmd_len++] = 0;
				break;
			}

      cmd_buf[cmd_len++] = receive_char;
		  uart_putc (receive_char);
		}	
		uart_puts_P (PSTR("\n\r"));
		tcp_entry[index].status =  ACK_FLAG;

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
				} else if (!strncmp(cmd_buf, "LIST", 4)) {
					printf_packet(index, "150 Opening ASCII mode data connection for list\r\n");

					self->state = STATE_LIST_START;

					printf_packet(self->data_idx, "first line\r\n");
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

		return;
	}

	return;
}

void ftpd_init (void)
{
	add_tcp_app (FTPD_PORT, (void(*)(unsigned char))ftp_ctrl);
}

