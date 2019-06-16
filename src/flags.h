/*
 * cbmdisk - simple sd card based IEEE-488 CBM floppy emulator 
 * Copyright (C) 2015, 2019 Guenter Bartsch
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
   flags.h: Definitions for some global flags

*/

#ifndef FLAGS_H
#define FLAGS_H

#ifdef __AVR__
/* GPIOR0 is a bit-addressable register reserved for user data */
#  define globalflags (GPIOR0)
#else
/* Global flags, variable defined in doscmd.c */
extern uint8_t globalflags;
#endif

/** flag values **/
/* transient flags */
#define VC20MODE         (1<<0)
#define AUTOSWAP_ACTIVE  (1<<2)
#define SWAPLIST_ASCII   (1<<5)

/* permanent (EEPROM-saved) flags */
/* 1<<1 was JIFFY_ENABLED */
#define EXTENSION_HIDING (1<<3)
#define POSTMATCH        (1<<4)

/* Disk image-as-directory mode, defined in fileops.c */
extern uint8_t image_as_dir;

#define IMAGE_DIR_NORMAL 0
#define IMAGE_DIR_DIR    1
#define IMAGE_DIR_BOTH   2

#endif
