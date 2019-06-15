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
 */

#ifndef _CONFIG_H_
#define _CONFIG_H_	

#include <avr/io.h>
#include <util/delay.h>
#include <avr/eeprom.h>
#include "avrcompat.h"

#define PROJECT "cbmdisk"
#define VERSION "0.1"
#define LONGVERSION VERSION

// Timertakt intern oder extern
#define EXTCLOCK 0 //0=Intern 1=Externer Uhrenquarz

// serial port
#define BAUDRATE 38400

/*
 * NODISKEMU
 */

#define CONFIG_COMMAND_BUFFER_SIZE 120
#define CONFIG_ERROR_BUFFER_SIZE   100
#define CONFIG_BUFFER_COUNT         15
#define CONFIG_MAX_PARTITIONS        2
#define MAX_DRIVES                   8
#define CONFIG_SD_AUTO_RETRIES      10

#define CONFIG_P00CACHE_SIZE      9000
/* P00 name cache is in bss by default */
#ifndef P00CACHE_ATTRIB
#  define P00CACHE_ATTRIB
#endif

#define CONFIG_HAVE_IEEE

#define HAVE_SD
#define SD_CHANGE_HANDLER     ISR(PCINT1_vect)
#define SD_SUPPLY_VOLTAGE (1L<<20)  /* 3.2V - 3.3V */

/* 250 kHz slow, 2 MHz fast */
#define SPI_DIVISOR_SLOW 64
#define SPI_DIVISOR_FAST 8
 
static inline void sdcard_interface_init(void) {
  DDRB   &= ~_BV(PB0);            /* card detect */
  PORTB  |=  _BV(PB0);
  DDRB   &= ~_BV(PB1);            /* write protect  */
  PORTB  |=  _BV(PB1);
  PCMSK1 |=  _BV(PCINT8);         /* card change interrupt */
  PCICR  |=  _BV(PCIE1);
}

static inline uint8_t sdcard_detect(void) {
  return (!(PINB & _BV(PB0)));
}

static inline uint8_t sdcard_wp(void) {
  return (PINB & _BV(PB1));
}

static inline uint8_t device_hw_address(void) {
  return 8;
}
static inline void device_hw_address_init(void) {
  return;
}

static inline __attribute__((always_inline)) void sdcard_set_ss(uint8_t state) {
  if (state)
    SPI_PORT |= SPI_SS;
  else
    SPI_PORT &= ~SPI_SS;
}

#define set_sd_led(x) do {} while (0)

static inline void leds_init(void) {
  DDRC |= _BV(PC7);
}

static inline __attribute__((always_inline)) void set_busy_led(uint8_t state) {
  if (state)
    PORTC |= _BV(PC7);
  else
    PORTC &= ~_BV(PC7);
}

#ifdef CONFIG_HAVE_IEEE
#  define HAVE_IEEE_POOR_MENS_VARIANT
#  define IEEE_ATN_INT          INT0    /* ATN interrupt (required!) */
#  define IEEE_ATN_INT0
#  define IEEE_INPUT_ATN        PIND    /* ATN */
#  define IEEE_PORT_ATN         PORTD
#  define IEEE_DDR_ATN          DDRD
#  define IEEE_PIN_ATN          PD2
#  define IEEE_INPUT_NDAC       PINC    /* NDAC */
#  define IEEE_PORT_NDAC        PORTC
#  define IEEE_DDR_NDAC         DDRC
#  define IEEE_PIN_NDAC         PC2
#  define IEEE_INPUT_NRFD       PINC    /* NRFD */
#  define IEEE_PORT_NRFD        PORTC
#  define IEEE_DDR_NRFD         DDRC
#  define IEEE_PIN_NRFD         PC1
#  define IEEE_INPUT_DAV        PINC    /* DAV */
#  define IEEE_PORT_DAV         PORTC
#  define IEEE_DDR_DAV          DDRC
#  define IEEE_PIN_DAV          PC0
#  define IEEE_INPUT_EOI        PINC    /* EOI */
#  define IEEE_PORT_EOI         PORTC
#  define IEEE_DDR_EOI          DDRC
#  define IEEE_PIN_EOI          PC5
#  define IEEE_INPUT_IFC        PINC    /* IFC */
#  define IEEE_PORT_IFC         PORTC
#  define IEEE_DDR_IFC          DDRC
#  define IEEE_PIN_IFC          PC3
#  define IEEE_BIT_IFC          _BV(IEEE_PIN_IFC)
#  define IEEE_D_PIN            PINA    /* Data */
#  define IEEE_D_PORT           PORTA
#  define IEEE_D_DDR            DDRA

static inline void ieee_interface_init(void) {
//  IEEE_PORT_TE  &= (uint8_t) ~ IEEE_BIT_TE;         // Set TE low
//  IEEE_DDR_TE   |= IEEE_BIT_TE;                     // Define TE  as output
  IEEE_PORT_ATN |= _BV(IEEE_PIN_ATN);               // Enable pull-up for ATN
  IEEE_PORT_IFC |= IEEE_BIT_IFC;                    // Enable pull-up for IFC
  IEEE_DDR_ATN  &= (uint8_t) ~ _BV(IEEE_PIN_ATN);   // Define ATN as input
  IEEE_DDR_IFC  &= (uint8_t) ~ IEEE_BIT_IFC;        // Define IFC as input
}

#endif // CONFIG_HAVE_IEEE

/*
 * arch-eeprom.h
 */

/* AVR: Set EEPROM address pointer to the dummy entry */
static inline void eeprom_safety(void) {
  EEAR = 0;
}

#endif //_CONFIG_H


