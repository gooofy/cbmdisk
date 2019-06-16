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

   fatops.h: Definitions for the FAT operations

*/

#ifndef FATOPS_H
#define FATOPS_H

#include "buffers.h"
#include "dirent.h"
#include "wrapops.h"
#include "ff.h"

/* API */
void     fatops_init(uint8_t preserve_dir);
void     parse_error(FRESULT res, uint8_t readflag);
uint8_t  fat_delete(path_t *path, cbmdirent_t *dent);
uint8_t  fat_chdir(path_t *path, cbmdirent_t *dent);
void     fat_mkdir(path_t *path, uint8_t *dirname);
void     fat_open_read(path_t *path, cbmdirent_t *filename, buffer_t *buf);
void     fat_open_write(path_t *path, cbmdirent_t *filename, uint8_t type, buffer_t *buf, uint8_t append);
uint8_t  fat_getdirlabel(path_t *path, uint8_t *label);
uint8_t  fat_getid(path_t *path, uint8_t *id);
uint16_t fat_freeblocks(uint8_t part);
uint8_t  fat_opendir(dh_t *dh, path_t *dir);
int8_t   fat_readdir(dh_t *dh, cbmdirent_t *dent);
void     fat_read_sector(buffer_t *buf, uint8_t part, uint8_t track, uint8_t sector);
void     fat_write_sector(buffer_t *buf, uint8_t part, uint8_t track, uint8_t sector);
void     format_dummy(uint8_t drive, uint8_t *name, uint8_t *id);

extern const fileops_t fatops;
extern uint8_t file_extension_mode;

/* Generic helpers */
uint8_t image_unmount(uint8_t part);
uint8_t image_chdir(path_t *path, cbmdirent_t *dent);
void    image_mkdir(path_t *path, uint8_t *dirname);
uint8_t image_read(uint8_t part, DWORD offset, void *buffer, uint16_t bytes);
uint8_t image_write(uint8_t part, DWORD offset, void *buffer, uint16_t bytes, uint8_t flush);

typedef enum { IMG_UNKNOWN, IMG_IS_DISK } imgtype_t;

imgtype_t check_imageext(uint8_t *name);

#endif
