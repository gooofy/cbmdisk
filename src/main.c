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

#include <avr/io.h>
#include <util/delay.h>

#include "config.h"

#include "timer.h"
#include "uart.h"

#include "spi.h"
#include "ieee.h"
#include "buffers.h"
#include "sdcard.h"
#include "filesystem.h"
#include "bus.h"
#include "eeprom-conf.h"
#include "ff.h"
#include "parser.h"
#include "doscmd.h"
#include "fileops.h"

int main(void)
{  

    leds_init();

    set_busy_led(1);

    uart_init(); 
    spi_init(SPI_SPEED_SLOW);
    timer_init();
    ieee_interface_init();

    // enable interrupts
    sei(); 

    _delay_ms(500);

    uart_puts_P(PSTR("\n\r" PROJECT " " VERSION "\n\r"));

    // NODISKEMU buffers / sdcard access
    buffers_init();
    bus_init();
    disk_init(); 
    read_configuration();

    filesystem_init(0);
    //change_init();

    uart_puts_P(PSTR("\n\rREADY.\n\r"));

    set_busy_led(0);

    bus_mainloop();

    printf("mainloop done ?!\r\n");

    while (1);

    return(0);
}

