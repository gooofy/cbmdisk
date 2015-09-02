/* NODISKEMU - SD/MMC to IEEE-488 interface/controller
   Copyright (C) 2007-2014  Ingo Korb <ingo@akana.de>

   NODISKEMU is a fork of sd2iec by Ingo Korb (et al.), http://sd2iec.de

   Inspired by MMC2IEC by Lars Pontoppidan et al.

   FAT filesystem access based on code from ChaN and Jim Brain, see ff.c|h.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License only.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA


   uart.h: Definitions for the UART access routines

*/

#ifndef UART_H
#define UART_H

#include <stdio.h>
#include "progmem.h"

#define CONFIG_UART_BAUDRATE 38400

void uart_init(void);
unsigned char uart_getc(void);
void uart_putc(char c);
void uart_puthex(uint8_t num);
void uart_trace(void *ptr, uint16_t start, uint16_t len);
void uart_flush(void);
void uart_puts_P(const char *text);
void uart_puts(const char *text);
void uart_putcrlf(void);

#ifdef __AVR__
#  define printf(str,...) printf_P(PSTR(str), ##__VA_ARGS__)
#endif

#endif
