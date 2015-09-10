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
 * Integer definitions for ff, based on example code from ChaN */

#ifndef _INTEGER

#include <stdint.h>

/* These types are assumed as 16-bit or larger integer */
typedef int16_t  INT;
typedef uint16_t UINT;

/* These types are assumed as 8-bit integer */
typedef int8_t  CHAR;
typedef uint8_t UCHAR;
typedef uint8_t BYTE;

/* These types are assumed as 16-bit integer */
typedef int16_t  SHORT;
typedef uint16_t USHORT;
typedef uint16_t WORD;

/* These types are assumed as 32-bit integer */
typedef int32_t  LONG;
typedef uint32_t ULONG;
typedef uint32_t DWORD;

/* Boolean type */
typedef enum { FALSE = 0, TRUE } BOOL;

#define _INTEGER
#endif
