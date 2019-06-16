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
   errormsg.c: Generates Commodore-compatible error messages

*/

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "buffers.h"
#include "diskio.h"
#include "eeprom-conf.h"
#include "fatops.h"
#include "flags.h"
#include "progmem.h"
#include "ustring.h"
#include "utils.h"
#include "uart.h"
#include "errormsg.h"

#define DEBUG_ERRORS

#ifdef DEBUG_ERRORS
# define DEBUG_PUTS_P(x) uart_puts_P(PSTR(x))
# define DEBUG_PUTHEX(x) uart_puthex(x)
# define DEBUG_PUTC(x)   uart_putc(x)
# define DEBUG_FLUSH()   uart_flush()
#else
# define DEBUG_PUTS_P(x) do {} while (0)
# define DEBUG_PUTHEX(x) do {} while (0)
# define DEBUG_PUTC(x) do {} while (0)
# define DEBUG_FLUSH() do {} while (0)
#endif

uint8_t current_error;
uint8_t error_buffer[CONFIG_ERROR_BUFFER_SIZE];

/// Version number string, will be added to message 73
const char PROGMEM versionstr[] = "cbmdisk V" VERSION;

/// Long version string, used for message 9
const char PROGMEM longverstr[] = LONGVERSION;

#define EC(x) x+0x80

/// Abbreviations used in the main error strings
static const PROGMEM uint8_t abbrevs[] = {
  EC(0), 'F','I','L','E',
  EC(1), 'R','E','A','D',
  EC(2), 'W','R','I','T','E',
  EC(3), ' ','E','R','R','O','R',
  EC(4), ' ','N','O','T',' ',
  EC(5), 'D','I','S','K',' ',
  EC(6), 'O','P','E','N',
  EC(7), 'R','E','C','O','R','D',
  EC(8), 'P','A','R','T','I','T','I','O','N',' ',
  EC(9), 'S','E','L','E','C','T','E','D',
  EC(10), 'I','L','L','E','G','A','L',
  EC(11), ' ','T','O','O',' ',
  EC(12), 'N','O',' ',
  EC(127)
};

/// Error string table
static const PROGMEM uint8_t messages[] = {
  EC(00),
    ' ','O','K',
  EC(01),
    0,'S',' ','S','C','R','A','T','C','H','E','D',
  EC(02),
    8,9,
  EC(20), EC(21), EC(22), EC(23), EC(24), EC(27),
    1,3,
  EC(25), EC(28),
    2,3,
  EC(26),
    2,' ','P','R','O','T','E','C','T',' ','O','N',
  EC(29),
    5,'I','D',' ','M','I','S','M','A','T','C','H',
  EC(30), EC(31), EC(32), EC(33), EC(34),
    'S','Y','N','T','A','X',3,
  EC(39), EC(62),
    0,4,'F','O','U','N','D',
  EC(50),
    7,4,'P','R','E','S','E','N','T',
  EC(51),
    'O','V','E','R','F','L','O','W',' ','I','N',' ',7,
  EC(52),
    0,11,'L','A','R','G','E',
  EC(60),
    2,' ',0,' ',6,
  EC(61),
    0,4,6,
  EC(63),
    0,' ','E','X','I','S','T','S',
  EC(64),
    0,' ','T','Y','P','E',' ','M','I','S','M','A','T','C','H',
  EC(65),
    12,'B','L','O','C','K',
  EC(66), EC(67),
    10,' ','T','R','A','C','K',' ','O','R',' ','S','E','C','T','O','R',
  EC(70),
    12,'C','H','A','N','N','E','L',
  EC(71),
    'D','I','R',3,
  EC(72),
    5,'F','U','L','L',
  EC(74),
    'D','R','I','V','E',4,1,'Y',
  EC(77),
    9,' ',8,10,
  EC(78),
    'B','U','F','F','E','R',11,'S','M','A','L','L',
  EC(79),
    'I','M','A','G','E',' ',0,' ','I','N','V','A','L','I','D',
  EC(99),
    'C','L','O','C','K',' ','U','N','S','T','A','B','L','E',
  EC(127)
};

static uint8_t *appendmsg(uint8_t *msg, const uint8_t *table, const uint8_t entry) {
  uint8_t i,tmp;

  i = 0;
  do {
    tmp = pgm_read_byte(table+i++);
    if (tmp == EC(entry) || tmp == EC(127))
      break;
  } while (1);

  if (tmp == EC(127)) {
    /* Unknown error */
    *msg++ = '?';
  } else {
    /* Skip over remaining error numbers */
    while (pgm_read_byte(table+i) >= EC(0)) i++;

    /* Copy error string to buffer */
    do {
      tmp = pgm_read_byte(table+i++);

      if (tmp < 32) {
        /* Abbreviation found, handle by recursion */
        msg = appendmsg(msg,abbrevs,tmp);
        continue;
      }

      if (tmp < EC(0))
        /* Don't copy error numbers */
        *msg++ = tmp;
    } while (tmp < EC(0));
  }

  return msg;
}

static uint8_t *appendbool(uint8_t *msg, uint8_t ch, uint8_t value) {
  if (ch)
    *msg++ = ch;

  if (value)
    *msg++ = '+';
  else
    *msg++ = '-';

  *msg++ = ':';

  return msg;
}

void set_error(uint8_t errornum) {
  set_error_ts(errornum,0,0);
}

void set_error_ts(uint8_t errornum, uint8_t track, uint8_t sector) {
  uint8_t *msg = error_buffer;
  uint8_t i = 0;

  if (errnum) {
    DEBUG_PUTHEX(errornum);
    DEBUG_PUTHEX(track);
    DEBUG_PUTHEX(sector);
    DEBUG_PUTS_P("ERRTS\r\n");
  }

  current_error = errornum;
  buffers[ERRORBUFFER_IDX].data     = error_buffer;
  buffers[ERRORBUFFER_IDX].lastused = 0;
  buffers[ERRORBUFFER_IDX].position = 0;
  memset(error_buffer,0,sizeof(error_buffer));

  msg = appendnumber(msg,errornum);
  *msg++ = ',';

  if (errornum == ERROR_STATUS) {
    switch(sector) {
    case 0:
    default:
      *msg++ = 'E';
      msg = appendnumber(msg, file_extension_mode);
      msg = appendbool(msg, 0, globalflags & EXTENSION_HIDING);

      msg = appendbool(msg, '*', globalflags & POSTMATCH);

      *msg++ = 'I';
      msg = appendnumber(msg, image_as_dir);

      *msg++ = ':';
      *msg++ = 'R';
      ustrcpy(msg, rom_filename);
      msg += ustrlen(rom_filename);

      break;
    case 1: // Drive Config
      *msg++ = 'D';
      while(i < 8) {
        if(map_drive(i) != 0x0f) {
          *msg++ = ':';
          msg = appendnumber(msg,i);
          *msg++ = '=';
          msg = appendnumber(msg,map_drive(i));
        }
        i++;
      }
      break;
    }

  } else if (errornum == ERROR_LONGVERSION || errornum == ERROR_DOSVERSION) {
    /* Start with the name and version number */
    while ((*msg++ = pgm_read_byte(versionstr+i++))) ;

    /* Append the long version if requested */
    if (errornum == ERROR_LONGVERSION) {
      i = 0;
      msg--;
      while ((*msg++ = toupper((int)pgm_read_byte(longverstr+i++)))) ;
    }

    msg--;
  } else {
    msg = appendmsg(msg,messages,errornum);
  }
  *msg++ = ',';

  msg = appendnumber(msg,track);
  *msg++ = ',';

  msg = appendnumber(msg,sector);
  *msg = 13;

  buffers[ERRORBUFFER_IDX].lastused = msg - error_buffer;

}

/* Callback for the error channel buffer */
uint8_t set_ok_message(buffer_t *buf) {
  set_error(0);
  return 0;
}
