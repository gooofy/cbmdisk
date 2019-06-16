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
 */

#ifndef ATA_H_
#define ATA_H_

/* ATA command */
#define ATA_CMD_RESET        0x08 /* DEVICE RESET */
#define ATA_CMD_READ         0x20 /* READ SECTOR(S) */
#define ATA_CMD_WRITE        0x30 /* WRITE SECTOR(S) */
#define ATA_CMD_IDENTIFY     0xec /* DEVICE IDENTIFY */
#define ATA_CMD_SETFEATURES  0xef /* SET FEATURES */
#define ATA_CMD_SPINDOWN     0xe0
#define ATA_CMD_SPINUP       0xe1
#define ATA_CMD_READ_EXT     0x24
#define ATA_CMD_WRITE_EXT    0x34

/* ATA register bit definitions */
#define ATA_LBA3_LBA         0x40

#define ATA_STATUS_BSY       0x80
#define ATA_STATUS_DRDY      0x40
#define ATA_STATUS_DRQ       0x08
#define ATA_STATUS_ERR       0x01

#define ATA_DEVCTRL_SRST     0x40
#define ATA_DEVCTRL_NIEN     0x20

/* Control Ports */
#define ATA_PORT_CTRL_OUT    PORTF
#define ATA_PORT_CTRL_DDR    DDRF
#define ATA_PORT_DATA_HI_OUT PORTC
#define ATA_PORT_DATA_HI_DDR DDRC
#define ATA_PORT_DATA_HI_IN  PINC
#define ATA_PORT_DATA_LO_OUT PORTA
#define ATA_PORT_DATA_LO_DDR DDRA
#define ATA_PORT_DATA_LO_IN  PINA

/* Bit definitions for Control Port */

#define ATA_PIN_RESET        (1<<PIN5)
#define ATA_PIN_WR           (1<<PIN7)
#define ATA_PIN_RD           (1<<PIN6)
#define ATA_PIN_A0           (1<<PIN2)
#define ATA_PIN_A1           (1<<PIN3)
#define ATA_PIN_A2           (1<<PIN4)
#define ATA_PIN_CS0          (1<<PIN0)
#define ATA_PIN_CS1          (1<<PIN1)

#define ATA_REG_BASE         (ATA_PIN_RD | ATA_PIN_WR | ATA_PIN_RESET)
#define ATA_REG_DATA             (ATA_REG_BASE | ATA_PIN_CS1)

#define ATA_REG_ERROR        (ATA_REG_BASE | ATA_PIN_CS1 | ATA_PIN_A0)
#define ATA_REG_FEATURES     ATA_REG_ERROR
#define ATA_REG_COUNT        (ATA_REG_BASE | ATA_PIN_CS1 | ATA_PIN_A1)
#define ATA_REG_LBA0         (ATA_REG_BASE | ATA_PIN_CS1 | ATA_PIN_A1 | ATA_PIN_A0)
#define ATA_REG_LBA1         (ATA_REG_BASE | ATA_PIN_CS1 | ATA_PIN_A2)
#define ATA_REG_LBA2         (ATA_REG_BASE | ATA_PIN_CS1 | ATA_PIN_A2 | ATA_PIN_A0)
#define ATA_REG_LBA3         (ATA_REG_BASE | ATA_PIN_CS1 | ATA_PIN_A2 | ATA_PIN_A1)
#define ATA_REG_STATUS       (ATA_REG_BASE | ATA_PIN_CS1 | ATA_PIN_A2 | ATA_PIN_A1 | ATA_PIN_A0)
#define ATA_REG_CMD          ATA_REG_STATUS
#define ATA_REG_ALTSTAT      (ATA_REG_BASE | ATA_PIN_CS0 | ATA_PIN_A2 | ATA_PIN_A1)
#define ATA_REG_DEVCTRL      (ATA_REG_BASE | ATA_PIN_CS0 | ATA_PIN_A2 | ATA_PIN_A1 | ATA_PIN_A0)

#define ATA_DEV_MASTER       0x00
#define ATA_DEV_SLAVE        0x10

#define STA_48BIT            0x08
#define STA_FIRSTTIME        0x80

#define RESET_DELAY          100   /* ms to hold RESET line low to init CF and IDE */

/* These functions are weak-aliased to disk_... */
void ata_init(void);
DSTATUS ata_initialize (BYTE drv);
DSTATUS ata_status (BYTE drv);
DRESULT ata_read (BYTE drv, BYTE *data, DWORD sector, BYTE count);
DRESULT ata_write (BYTE drv, const BYTE *data, DWORD sector, BYTE count);
#if _USE_IOCTL != 0
DRESULT ata_ioctl (BYTE drv, BYTE ctrl, void *buff);
#endif
DRESULT ata_getinfo(BYTE drv, BYTE page, void *buffer);


#endif /*ATA_H_*/
