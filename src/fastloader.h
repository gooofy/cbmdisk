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

   fastloader.h: Definitions for the high level fast loader handling

*/

#ifndef FASTLOADER_H
#define FASTLOADER_H

#define FL_NONE              0
#define FL_TURBODISK         1
#define FL_FC3_LOAD          2
#define FL_FC3_SAVE          3
#define FL_DREAMLOAD         4
#define FL_DREAMLOAD_OLD     5
#define FL_FC3_FREEZED       6
#define FL_ULOAD3            7
#define FL_GI_JOE            8
#define FL_EPYXCART          9
#define FL_GEOS_S1_64       10
#define FL_GEOS_S1_128      11
#define FL_GEOS_S23_1541    12
#define FL_GEOS_S23_1571    13
#define FL_GEOS_S23_1581    14
#define FL_WHEELS_S1_64     15
#define FL_WHEELS_S1_128    16
#define FL_WHEELS_S2        17
#define FL_WHEELS44_S2      18
#define FL_WHEELS44_S2_1581 19
#define FL_NIPPON           20
#define FL_AR6_1581_LOAD    21
#define FL_AR6_1581_SAVE    22
#define FL_ELOAD1           23
#define FL_FC3_OLDFREEZED   24

#ifndef __ASSEMBLER__

extern uint8_t detected_loader;
extern volatile uint8_t fl_track;
extern volatile uint8_t fl_sector;
extern uint8_t (*fast_send_byte)(uint8_t byte);
extern uint8_t (*fast_get_byte)(void);

void load_turbodisk(uint8_t);
void load_fc3(uint8_t freezed);
void load_fc3oldfreeze(uint8_t);
void save_fc3(uint8_t);
void load_dreamload(uint8_t);
void load_uload3(uint8_t);
void load_eload1(uint8_t);
void load_gijoe(uint8_t);
void load_epyxcart(uint8_t);
void load_geos(uint8_t);
void load_geos_s1(uint8_t version);
void load_wheels_s1(uint8_t version);
void load_wheels_s2(uint8_t);
void load_nippon(uint8_t);
void load_ar6_1581(uint8_t);
void save_ar6_1581(uint8_t);

int16_t dolphin_getc(void);
uint8_t dolphin_putc(uint8_t data, uint8_t with_eoi);
void load_dolphin(void);
void save_dolphin(void);

# ifdef PARALLEL_ENABLED
extern volatile uint8_t parallel_rxflag;
static inline void parallel_clear_rxflag(void) { parallel_rxflag = 0; }
# else
#  define parallel_rxflag 0
static inline void parallel_clear_rxflag(void) {}
# endif

#endif
#endif
