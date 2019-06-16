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
#define DEBUG_IEEE 

#ifdef DEBUG_IEEE
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

#include "config.h"

#include <avr/io.h>
#include <avr/pgmspace.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <util/delay.h>

#include "uart.h"
#include "buffers.h"
#include "d64ops.h"
#include "diskio.h"
#include "doscmd.h"
#include "fatops.h"
#include "fileops.h"
#include "filesystem.h"
#include "ieee.h"
#include "fastloader.h"
#include "errormsg.h"
#include "ctype.h"
#include "timer.h"

// -------------------------------------------------------------------------
//  Global variables
// -------------------------------------------------------------------------

uint8_t device_address;                 // Current device address
volatile bool ieee488_TE75160;          // direction set for data lines
volatile bool ieee488_TE75161;          // direction set for ctrl lines


#define PRESERVE_CURRENT_DIRECTORY      1
#define CHANGE_TO_ROOT_DIRECTORY        0

#define IEEE_EOI        0x100
#define IEEE_NO_DATA    0x200

#define TE_LISTEN       0
#define TE_TALK         1

#define DC_BUSMASTER    0
#define DC_DEVICE       1

// Upper three bit commands with attached device number
#define IEEE_LISTEN      0x20    // 0x20 - 0x3E
#define IEEE_UNLISTEN    0x3F
#define IEEE_TALK        0x40    // 0x40 - 0x5E
#define IEEE_UNTALK      0x5F

// Upper four bit commands with attached secondary address
#define IEEE_SECONDARY   0x60
#define IEEE_CLOSE       0xE0
#define IEEE_OPEN        0xF0


void    ieee488_Init(void);
void    ieee488_MainLoop(void);
void    ieee488_CtrlPortsTalk(void);    // Switch bus driver to talk mode
void    ieee488_CtrlPortsListen(void);  // Switch bus driver to listen mode

static inline void ieee488_SetEOI(bool x);
static inline void ieee488_SetDAV(bool x);
static inline void ieee488_SetNDAC(bool x);
static inline void ieee488_SetNRFD(bool x);

#ifndef HAVE_IEC
uint8_t detected_loader = 0;
#endif

#ifdef IEEE_INPUT_IFC
static inline void ieee488_InitIFC(void) {
  IEEE_DDR_IFC &= ~_BV(IEEE_PIN_IFC);           // IFC as input
  IEEE_PORT_IFC |= _BV(IEEE_PIN_IFC);           // enable pull-up
}

static inline uint8_t ieee488_IFC(void) {
  return IEEE_INPUT_IFC & _BV(IEEE_PIN_IFC);
}


#else
static inline void ieee488_InitIFC(void) {}

static inline uint8_t ieee488_IFC(void) {
  return 0xFF;
}

#endif // #ifdef IEEE_INPUT_IFC


static inline uint8_t ieee488_ATN(void) {
  return IEEE_INPUT_ATN & _BV(IEEE_PIN_ATN);
}


static inline uint8_t ieee488_NDAC(void) {
  return IEEE_INPUT_NDAC & _BV(IEEE_PIN_NDAC);
}


static inline uint8_t ieee488_NRFD(void) {
  return IEEE_INPUT_NRFD & _BV(IEEE_PIN_NRFD);
}


static inline uint8_t ieee488_DAV(void) {
  return IEEE_INPUT_DAV & _BV(IEEE_PIN_DAV);
}


static inline uint8_t ieee488_EOI(void) {
  return IEEE_INPUT_EOI & _BV(IEEE_PIN_EOI);
}

static inline void ieee488_SetEOI(bool x) {
  // bus driver changes flow direction of EOI from transmit
  // to receive on ATN, so simulate OC output here
  if (x) {
    IEEE_PORT_EOI |=  _BV(IEEE_PIN_EOI);        // enable pull-up for EOI
    IEEE_DDR_EOI  &= ~_BV(IEEE_PIN_EOI);        // EOI as input
  } else {
    IEEE_PORT_EOI &= ~_BV(IEEE_PIN_EOI);        // EOI = 0
    IEEE_DDR_EOI  |=  _BV(IEEE_PIN_EOI);        // EOI as output
  }
}


#ifdef HAVE_7516X
// Device with 75160/75161 bus drivers

#ifdef IEEE_PIN_D7

static inline void ieee488_SetData(uint8_t data) {
  IEEE_D_PORT &= 0b10000000;
  IEEE_D_PORT |= (~data) & 0b01111111;
  if (data & 128) IEEE_PORT_D7 &= ~_BV(IEEE_PIN_D7);
  else            IEEE_PORT_D7 |=  _BV(IEEE_PIN_D7);
}

static inline uint8_t ieee488_Data(void) {
   uint8_t data = IEEE_D_PIN;
   if (bit_is_set(IEEE_INPUT_D7, IEEE_PIN_D7))
      data |= 0b10000000;
   else
      data &= 0b01111111;
   return ~data;
}

static inline void ieee488_DataListen(void) {
  // This code assumes that D7 on this port is also used as input!
  IEEE_D_DDR  = 0x00;                   // data lines as input
  IEEE_DDR_D7 &= _BV(IEEE_PIN_D7);
  ieee488_TE75160 = TE_LISTEN;
}


static inline void ieee488_DataTalk(void) {
  IEEE_D_PORT = 0b01111111;             // release data lines
  IEEE_D_DDR  = 0b01111111;             // data lines as output
  IEEE_PORT_D7 |= _BV(IEEE_PIN_D7);
  IEEE_DDR_D7  |= _BV(IEEE_PIN_D7);
  ieee488_TE75160 = TE_TALK;
}
#else
#ifdef IEEE_D_DDR
static inline uint8_t ieee488_Data(void) {
  return ~IEEE_D_PIN;
}


static inline void ieee488_SetData(uint8_t data) {
  IEEE_D_PORT = ~data;
}


static inline void ieee488_DataListen(void) {
  IEEE_D_DDR  = 0x00;                   // data lines as input
  ieee488_TE75160 = TE_LISTEN;
}


static inline void ieee488_DataTalk(void) {
  IEEE_D_PORT = 0xFF;                   // release data lines
  IEEE_D_DDR  = 0xFF;                   // data lines as output
  ieee488_TE75160 = TE_TALK;
}
#else
#ifdef IEEE_DATA_READ
#include "MCP23S17.h"

static inline void ieee488_DataListen(void) {
  mcp23s17_Write(IEEE_DDR_DATA, 0xFF);  // data lines as input
  IEEE_PORT_TED &= ~_BV(IEEE_PIN_TED);  // TED=0 (listen)
  ieee488_TE75160 = TE_LISTEN;
}


static inline void ieee488_DataTalk(void) {
  IEEE_PORT_TED |= _BV(IEEE_PIN_TED);   // TED=1 (talk)
  mcp23s17_Write(IEEE_DDR_DATA, 0x00);  // data lines as output
  ieee488_TE75160 = TE_TALK;
}


static inline uint8_t ieee488_Data(void) {
  // MCP23S17 configured for inverted polarity input (IPOL),
  // so no need to invert here
  return mcp23s17_Read(IEEE_DATA_READ);
}


static inline void ieee488_SetData(uint8_t data) {
  mcp23s17_Write(IEEE_DATA_WRITE, ~data);
}

#else
#error ieee488 data functions undefined
#endif
#endif
#endif


#ifdef IEEE_PORT_DC
static inline void ieee488_InitDC(void) {
  IEEE_DDR_DC |= _BV(IEEE_PIN_DC);      // DC as output
}


static inline void ieee488_SetDC(bool x) {
  if (x)
    IEEE_PORT_DC |=  _BV(IEEE_PIN_DC);
  else
    IEEE_PORT_DC &= ~_BV(IEEE_PIN_DC);
}

#else
#ifdef IEEE_DC_MCP23S17
#include "MCP23S17.h"
static inline void ieee488_InitDC(void) {
  // intentionally left blank
  // DC is initialized by mcp23s17_Init()
}


static inline void ieee488_SetDC(bool x) {
  if (x)
    mcp23s17_SetBit(IEEE_DC_MCP23S17);
  else
    mcp23s17_ClearBit(IEEE_DC_MCP23S17);
}
#else
#ifndef IEEE_PORT_DC
static inline void ieee488_InitDC(void) {
   // intentionally left blank
}

static inline void ieee488_SetDC(bool x) {
   // intentionally left blank
}
#else
#error ieee488_SetDC() / ieee488_InitDC undefined
#endif // #ifndef IEEE_PORT_DC
#endif // #ifdef IEEE_DC_MCP23S17
#endif // #ifdef IEEE_PORT_DC


static inline void ieee488_SetNDAC(bool x) {
  if (x)
    IEEE_PORT_NDAC |=  _BV(IEEE_PIN_NDAC);
  else
    IEEE_PORT_NDAC &= ~_BV(IEEE_PIN_NDAC);
}


static inline void ieee488_SetNRFD(bool x) {
  if (x)
    IEEE_PORT_NRFD |=  _BV(IEEE_PIN_NRFD);
  else
    IEEE_PORT_NRFD &= ~_BV(IEEE_PIN_NRFD);
}


static inline void ieee488_SetDAV(bool x) {
  if (x)
    IEEE_PORT_DAV |=  _BV(IEEE_PIN_DAV);
  else
    IEEE_PORT_DAV &= ~_BV(IEEE_PIN_DAV);
}


static inline void ieee488_SetTE(bool x) {
  if (x)
    IEEE_PORT_TE |=  _BV(IEEE_PIN_TE);
  else
    IEEE_PORT_TE &= ~_BV(IEEE_PIN_TE);
}

void ieee488_CtrlPortsListen(void) {
  IEEE_DDR_EOI &= ~_BV(IEEE_PIN_EOI);           // EOI  as input
  IEEE_DDR_DAV &= ~_BV(IEEE_PIN_DAV);           // DAV  as input
  ieee488_SetTE(TE_LISTEN);
  IEEE_DDR_NDAC |= _BV(IEEE_PIN_NDAC);          // NDAC as output
  IEEE_DDR_NRFD |= _BV(IEEE_PIN_NRFD);          // NRFD as output
  ieee488_TE75161 = TE_LISTEN;
}


void ieee488_CtrlPortsTalk(void) {
  IEEE_DDR_NDAC &= ~_BV(IEEE_PIN_NDAC);         // NDAC as input
  IEEE_DDR_NRFD &= ~_BV(IEEE_PIN_NRFD);         // NRFD as input
  IEEE_PORT_DAV |= _BV(IEEE_PIN_DAV);           // DAV high
  IEEE_PORT_EOI |= _BV(IEEE_PIN_EOI);           // EOI high
  ieee488_SetTE(TE_TALK);                       // TE=1 (talk)
  IEEE_DDR_DAV |= _BV(IEEE_PIN_DAV);            // DAV as output
  IEEE_DDR_EOI |= _BV(IEEE_PIN_EOI);            // EOI as output
  ieee488_TE75161 = TE_TALK;
}

#else /* ifdef HAVE_7516X */
#ifdef HAVE_IEEE_POOR_MENS_VARIANT

// -----------------------------------------------------------------------
//  Poor men's variant without IEEE bus drivers
// -----------------------------------------------------------------------
static inline uint8_t ieee488_Data(void) {
  return ~IEEE_D_PIN;
}


static inline void ieee488_SetData(uint8_t data) {
  // Pull lines low by outputting zero
  // pull lines high by defining them as input and enabling the pull-up
  IEEE_D_DDR  = data;
  IEEE_D_PORT = ~data;
}


static inline void ieee488_DataListen(void) {
  IEEE_D_DDR  = 0x00;                           // data lines as input
  IEEE_D_PORT = 0xFF;                           // enable pull-ups
  ieee488_TE75160 = TE_LISTEN;
}


static inline void ieee488_DataTalk(void) {
  IEEE_D_DDR  = 0x00;                           // data lines as input
  IEEE_D_PORT = 0xFF;                           // enable pull-ups
  ieee488_TE75160 = TE_TALK;
}


void ieee488_CtrlPortsListen(void) {
  ieee488_SetEOI(1);
  ieee488_SetDAV(1);
  ieee488_SetNDAC(1);
  ieee488_SetNRFD(1);
  ieee488_TE75161 = TE_LISTEN;
}


void ieee488_CtrlPortsTalk(void) {
  ieee488_SetEOI(1);
  ieee488_SetDAV(1);
  ieee488_SetNDAC(1);
  ieee488_SetNRFD(1);
  ieee488_TE75161 = TE_TALK;
}


static inline void ieee488_SetNDAC(bool x) {
  if(x) {                                       // Set NDAC high
    IEEE_DDR_NDAC &= ~_BV(IEEE_PIN_NDAC);       // NDAC as input
    IEEE_PORT_NDAC |= _BV(IEEE_PIN_NDAC);       // Enable pull-up
  } else {                                      // Set NDAC low
    IEEE_PORT_NDAC &= ~_BV(IEEE_PIN_NDAC);      // NDAC low
    IEEE_DDR_NDAC |= _BV(IEEE_PIN_NDAC);        // NDAC as output
  }
}

static inline void ieee488_SetNRFD(bool x) {
  if(x) {                                       // Set NRFD high
    IEEE_DDR_NRFD &= ~_BV(IEEE_PIN_NRFD);       // NRFD as input
    IEEE_PORT_NRFD |= _BV(IEEE_PIN_NRFD);       // Enable pull-up
  } else {                                      // Set NRFD low
    IEEE_PORT_NRFD &= ~_BV(IEEE_PIN_NRFD);      // NRFD low
    IEEE_DDR_NRFD |= _BV(IEEE_PIN_NRFD);        // NRFD as output
  }
}

static inline void ieee488_SetDAV(bool x) {
  if(x) {                                       // Set DAV high
    IEEE_DDR_DAV &= ~_BV(IEEE_PIN_DAV);         // DAV as input
    IEEE_PORT_DAV |= _BV(IEEE_PIN_DAV);         // Enable pull-up
  } else {                                      // Set DAV low
    IEEE_PORT_DAV &= ~_BV(IEEE_PIN_DAV);        // DAV low
    IEEE_DDR_DAV |= _BV(IEEE_PIN_DAV);          // DAV as output
  }
}

static inline void ieee488_SetTE(bool x) {
  // left intentionally blank
}

static inline void ieee488_InitDC(void) {
  // left intentionally blank
}

static inline void ieee488_SetDC(bool x) {
  // left intentionally blank
}

#else
#error No IEEE-488 low level routines defined
#endif
#endif

// TE=0: listen mode, TE=1: talk mode
volatile bool ieee488_TE75160; // bus driver for data lines
volatile bool ieee488_TE75161; // bus driver for control lines

/* Please note that the init-code is spread across two functions:
   ieee488_Init() follows below, but in src/avr/arch-config.h there
   is ieee_interface_init() also, which is aliased to bus_interface_init()
   there and called as such from main().
*/
void ieee488_Init(void) {
  device_hw_address_init();
  device_address = device_hw_address();
  ieee488_InitDC();
  ieee488_SetDC(DC_DEVICE);
#ifdef HAVE_7516X
  IEEE_DDR_TE  |= _BV(IEEE_PIN_TE);             // TE  as output
#endif
  IEEE_DDR_ATN &= ~_BV(IEEE_PIN_ATN);           // ATN as input
  ieee488_DataListen();
  ieee488_CtrlPortsListen();
}

void bus_init(void) __attribute__((weak, alias("ieee488_Init")));


void handle_card_changes(void) {
#ifdef HAVE_HOTPLUG
  if (disk_state != DISK_OK) {
    set_busy_led(1);
    // If the disk was changed the buffer contents are useless
    if (disk_state == DISK_CHANGED || disk_state == DISK_REMOVED) {
      free_multiple_buffers(FMB_ALL);
      change_init();
      filesystem_init(CHANGE_TO_ROOT_DIRECTORY);
    } else {
      // Disk state indicated an error, try to recover by initialising
      filesystem_init(PRESERVE_CURRENT_DIRECTORY);
    }
  }
#endif
}

#define STATE_IDLE                     1
#define STATE_WAITNATN                 2

#define STATE_LSN1                    11
#define STATE_LSN2                    12
#define STATE_LSN3                    13
#define STATE_LSN4                    14

#define STATE_TLK1                    21
#define STATE_TLK2                    22
#define STATE_TLK3                    23
#define STATE_TLK4                    24
#define STATE_TLK5                    25
#define STATE_TLK6                    26
#define STATE_TLK7                    27
#define STATE_TLK8                    28


#define DEVICE_STATE_IDLE             1
#define DEVICE_STATE_LISTENER         2
#define DEVICE_STATE_TALKER           3

/*
 * low level IEEE 488 interface functions used by FSM
 *
	  ieee488_CtrlPortsListen()
      ieee488_CtrlPortsTalk()

      ieee488_IFC()
      ieee488_ATN()

      ieee488_NDAC()
      ieee488_SetNDAC(x)
      ieee488_NRFD()
      ieee488_SetNRFD(x)
      ieee488_DAV()
      ieee488_SetDAV(x)
      ieee488_EOI() 

      ieee488_Data()
      ieee488_SetData(x)
      ieee488_DataListen()
      ieee488_DataTalk()
*/

#define TIMEOUT 16000

static uint8_t  fsm_state;
static uint8_t  data_out  = 65;
static uint8_t  eoi       =  0;
static uint16_t timeout   =  0;

static inline void goto_state(uint8_t state) {

    fsm_state = state;
    timeout = 0;

    switch(fsm_state) {
        case STATE_IDLE:
            ieee488_CtrlPortsListen();
            ieee488_SetNRFD(1);   
            ieee488_SetNDAC(1);
            ieee488_DataListen();
            //DEBUG_PUTS_P("IDLE\r\n");
            break;

        case STATE_WAITNATN:
            ieee488_SetNRFD(0);   
            ieee488_SetNDAC(0);
            break;

        case STATE_LSN1:
            ieee488_SetNRFD(1);   
            ieee488_SetNDAC(0);
            //DEBUG_PUTS_P("LSN1\r\n");
            break;
        case STATE_LSN2:
            ieee488_SetNRFD(0);
            ieee488_SetNDAC(0);
            //DEBUG_PUTS_P((PSTR("LSN2\r\n"));
            break;   
        case STATE_LSN3:
            ieee488_SetNRFD(0);
            ieee488_SetNDAC(1);
            //DEBUG_PUTS_P((PSTR("LSN3\r\n"));
            break;   
        case STATE_LSN4:
            ieee488_SetNRFD(0);
            ieee488_SetNDAC(0);
            //DEBUG_PUTS_P((PSTR("LSN4\r\n"));
            break;   

        case STATE_TLK1:
            ieee488_CtrlPortsTalk();
            ieee488_SetDAV(1);
            ieee488_SetEOI(0);   
            ieee488_DataTalk();
            break;

        case STATE_TLK2:
            break;

        case STATE_TLK3:
            ieee488_SetEOI(eoi);
            ieee488_SetData(data_out); 
            break;  

        case STATE_TLK4:
            ieee488_SetDAV(0);
            break;  

        case STATE_TLK5:
            break;

        case STATE_TLK6:
            ieee488_SetDAV(1);
            break;

        case STATE_TLK7:
            break;

        case STATE_TLK8:
            ieee488_CtrlPortsListen();
            ieee488_SetNDAC(1);
            ieee488_SetNRFD(1);   
            ieee488_DataListen();
            break;
    }
}

static inline bool fetch_next_byte (uint8_t cur_sa) {

    buffer_t *buf;

    eoi = 0;

    buf = find_buffer(cur_sa);

    if (!buf) {
        DEBUG_PUTHEX(cur_sa); DEBUG_PUTS_P("buf NULL!\r\n");
        return 0;
    } 

    data_out = buf->data[buf->position];
    eoi      = (buf->position == buf->lastused) && buf->sendeoi ? 0 : 1;

    if (buf->position == buf->lastused) {
        if (buf->sendeoi && cur_sa != 15 && !buf->recordlen &&
                buf->refill != directbuffer_refill) {
            buf->read = 0;
            DEBUG_PUTS_P("T8\r\n");
            return 0;
        } 
        if (buf->refill(buf)) {             // Refill buffer
            DEBUG_PUTS_P("T9\r\n");
            return 0;
        }
        // Search the buffer again, it can change when using large buffers
        buf = find_buffer(cur_sa);
    } else {
        buf->position++;
    }

    DEBUG_PUTHEX(data_out); 
    DEBUG_PUTC(':');DEBUG_PUTHEX(buf->position); DEBUG_PUTC('/'); DEBUG_PUTHEX(buf->lastused); DEBUG_PUTC(':');
    DEBUG_PUTHEX(buf->sendeoi); DEBUG_PUTC('/'); DEBUG_PUTHEX(eoi); DEBUG_PUTS_P(" FNB\r\n"); // FIXME: debug

    return 1;
}

static inline bool save_next_byte (uint8_t cur_sa, uint8_t c) {

    buffer_t *buf;

    buf = find_buffer(cur_sa);

    if (!buf) {
        DEBUG_PUTHEX(cur_sa); DEBUG_PUTS_P("Save buf NULL!\r\n");
        return 0;
    } 

    // Flush buffer if full
    if (buf->mustflush) {
        if (buf->refill(buf)) {
            DEBUG_PUTS_P("Save refill abort\r\n");
            return 0;
        }
        // Search the buffer again,
        // it can change when using large buffers
        buf = find_buffer(cur_sa);
    }

    buf->data[buf->position] = c;
    mark_buffer_dirty(buf);

#if DEBUG_BUS_DATA
    DEBUG_PUTC('{'); DEBUG_PUTHEX(c); DEBUG_PUTC('}'); // FIXME: debug
#endif

    if (buf->lastused < buf->position) buf->lastused = buf->position;
    buf->position++;

    // Mark buffer for flushing if position wrapped
    if (buf->position == 0) buf->mustflush = 1;

    // REL files must be syncronized on EOI
    if (buf->recordlen && !eoi) {
        if (buf->refill(buf)) {
            DEBUG_PUTS_P("Save refill abort2\r\n");
            return 0;
        }
    }

    return 1;
}

void ieee_mainloop_fsm(void) {

    uint8_t cmd=0;        // Received IEEE-488 command byte

    uint8_t device_state = DEVICE_STATE_IDLE;

    bool open_active     = 0;
    bool save_active     = 0;
    uint8_t cur_sa       = 0;
    buffer_t *buf        = NULL;
    bool do_tlk          = 0;
    bool last_byte       = 0;
    bool byte_pending    = 0;

    ieee488_InitIFC();

    goto_state (STATE_IDLE);

    for (;;) {

        // FIXME: implement card change handler
        // handle_card_changes();

        switch (fsm_state) {
            case STATE_IDLE:
                if (!ieee488_ATN())
                    goto_state (STATE_LSN1);
                break;

            case STATE_LSN1:
                if (!ieee488_IFC())
                    goto_state (STATE_IDLE);
                else if (!ieee488_DAV())
                    goto_state (STATE_LSN2);
                break;

            case STATE_LSN2:
                if (!ieee488_IFC()) {
                    goto_state (STATE_IDLE);
                } else {

                    cmd = ieee488_Data();
                    eoi = ieee488_EOI();

                    DEBUG_PUTC('['); DEBUG_PUTHEX(cmd); DEBUG_PUTS_P("]\r\n");

                    if (open_active) {
                        // Receive commands and filenames
                        if (command_length < CONFIG_COMMAND_BUFFER_SIZE)
                            command_buffer[command_length++] = (char) cmd;

                        if (!eoi) {

                            DEBUG_PUTS_P("CBUF:");

                            for (int8_t i = 0; i<command_length; i++)
                                DEBUG_PUTC(command_buffer[i]);

                            DEBUG_PUTS_P("\r\n");

                            if (cur_sa == 15) {
                                DEBUG_PUTS_P("DOSCMD\r\n");
                                parse_doscommand();
                            } else {
                                DEBUG_PUTHEX(cur_sa); 
                                DEBUG_PUTS_P("FOPN\r\n");
                                datacrc = 0xffff;                   // filename in command buffer
                                file_open(cur_sa);
                            }
                            open_active = 0;
                        }
                    } else if (save_active) {

                        if (cur_sa == 15) {
                            // Receive commands and filenames
                            if (command_length < CONFIG_COMMAND_BUFFER_SIZE)
                                command_buffer[command_length++] = (char) cmd;
                            if (!eoi) {
                                DEBUG_PUTS_P("DOSCMD\r\n");
                                parse_doscommand();
                            }
                        } else {
                            if (!save_next_byte(cur_sa, cmd)) {
                                DEBUG_PUTS_P("sberr\r\n");
                                save_active = 0;
                                device_state = DEVICE_STATE_IDLE;
                            }
                        }

                        if (!eoi) {
                            save_active = 0;
                        }

                    } else {

                        uint8_t cmd3   = cmd & 0b11100000;
                        uint8_t cmd4   = cmd & 0b11110000;
                        uint8_t Device = cmd & 0b00011111; // device number from cmd byte
                        uint8_t sa     = cmd & 0b00001111; // secondary address from cmd byte

                        if (cmd == IEEE_UNLISTEN) {           // UNLISTEN
                            DEBUG_PUTS_P("ULSN\r\n");
                            device_state = DEVICE_STATE_IDLE;

                        } else if (cmd == IEEE_UNTALK) {      // UNTALK
                            DEBUG_PUTS_P("UTLK\r\n");
                            do_tlk = 0;
                            //device_state = DEVICE_STATE_IDLE;
                            device_state = DEVICE_STATE_LISTENER;

                        } else if (cmd3 == IEEE_LISTEN) {     // LISTEN
                            DEBUG_PUTS_P("LSN\r\n");
                            if (Device == device_address) {
                                device_state = DEVICE_STATE_LISTENER;
                            } else {
                                device_state = DEVICE_STATE_IDLE;
                            }

                        } else if (cmd3 == IEEE_TALK) {       // TALK
                            DEBUG_PUTS_P("TLK\r\n");
                            if (Device == device_address) {
                                device_state = DEVICE_STATE_TALKER;
                            } else {
                                device_state = DEVICE_STATE_IDLE;
                            }

                        } else if (cmd4 == IEEE_SECONDARY) {  // DATA
                            DEBUG_PUTS_P("DTA\r\n");
                            cur_sa = sa;
                            if (device_state == DEVICE_STATE_TALKER) {

                                if (!byte_pending) {
                                    if (!fetch_next_byte (cur_sa)) {
                                        DEBUG_PUTHEX(data_out); DEBUG_PUTHEX(eoi); DEBUG_PUTS_P("TLK1 ERR\r\n"); // FIXME: debug
                                    }
                                    byte_pending = 1;
                                }

                                do_tlk    = 1;
                                last_byte = !eoi;

                            } else if (device_state == DEVICE_STATE_LISTENER) {
                                save_active = 1;
                                command_length = 0;
                            }

                        } else if (cmd4 == IEEE_CLOSE) {      // CLOSE
                            if (device_state != DEVICE_STATE_IDLE) {
                                DEBUG_PUTS_P("CLO\r\n");
                                byte_pending = 0;
                                if (sa == 15) {
                                    free_multiple_buffers(FMB_USER_CLEAN);
                                } else {
                                    buf = find_buffer(sa);
                                    if (buf != NULL) {
                                        buf->cleanup(buf);
                                        free_buffer(buf);
                                    }
                                }
                            }

                        } else if (cmd4 == IEEE_OPEN) {       // OPEN
                            if (device_state != DEVICE_STATE_IDLE) {
                                DEBUG_PUTHEX(sa); DEBUG_PUTS_P("OPN\r\n");

                                open_active = 1;
                                command_length = 0;
                                cur_sa = sa;
                            }

                        } else {
                            DEBUG_PUTS_P("UKN\r\n");
                        }
                    }

                    //DEBUG_FLUSH();
                    //_delay_ms(1000);
                    goto_state (STATE_LSN3);
                }
                break;

            case STATE_LSN3:
                if (!ieee488_IFC()) {
                    goto_state (STATE_IDLE);

                } else if (ieee488_DAV()) {

                    goto_state (STATE_LSN4);
                }
                break;

            case STATE_LSN4:
                if (!ieee488_IFC()) {
                    goto_state (STATE_IDLE);
                } else {
                    if (do_tlk) {
                        goto_state (STATE_TLK1);
                    } else if (device_state != DEVICE_STATE_IDLE) {
                        goto_state (STATE_LSN1);
                    } else {
                        goto_state (STATE_WAITNATN);
                    }
                }
                break;

            case STATE_WAITNATN:
                if (!ieee488_IFC() || ieee488_ATN()) {
                    goto_state (STATE_IDLE);
                }
                break;

            case STATE_TLK1:
                if (!ieee488_IFC()) {
                    goto_state (STATE_IDLE);
#if 0
                } else if (!ieee488_ATN()) {
                    DEBUG_PUTS_P("TLK1ATN\r\n"); // FIXME: debug
                    goto_state (STATE_TLK8);
#endif
                } else if (!ieee488_NDAC()) {
                    goto_state (STATE_TLK2);
                }
                break;

            case STATE_TLK2:
                if (!ieee488_IFC()) {
                    goto_state (STATE_IDLE);
#if 0
                } else if (!ieee488_ATN()) {
                    DEBUG_PUTS_P("TLK2ATN\r\n"); 
                    goto_state (STATE_TLK8);
#endif
#if 1
                } else if (ieee488_NDAC()) {
                    DEBUG_PUTS_P("TLK2NDAC\r\n"); 
                    goto_state (STATE_TLK8);
#endif
                } else if (ieee488_NRFD()) {
                    goto_state (STATE_TLK3);
                }
                break;

            case STATE_TLK3:
                if (!ieee488_IFC()) {
                    goto_state (STATE_IDLE);
                } else if (!ieee488_ATN()) {
                    DEBUG_PUTS_P("TLK3ATN\r\n"); 
                    goto_state (STATE_TLK8);
#if 1
                } else if (ieee488_NDAC()) {
                    DEBUG_PUTS_P("TLK3NDAC\r\n"); // FIXME: debug
                    goto_state (STATE_TLK8);
                } else if (!ieee488_NRFD()) {
                    DEBUG_PUTS_P("TLK3NRFD\r\n"); // FIXME: debug
                    goto_state (STATE_TLK8);
#endif
                } else {
                    goto_state (STATE_TLK4);
                }
                break;

            case STATE_TLK4:
                if (!ieee488_IFC()) {
                    goto_state (STATE_IDLE);
                } else if (!ieee488_ATN()) {
                    DEBUG_PUTS_P("TLK4ATN\r\n"); 
                    goto_state (STATE_TLK8);

#if 1
                } else if (ieee488_NDAC()) {
                    DEBUG_PUTS_P("TLK4NDAC\r\n"); 
                    goto_state (STATE_TLK8);
#endif

                } else if (!ieee488_NRFD()) {

                    if (last_byte) {
                        DEBUG_PUTS_P("TLK4 -> TLK5 finish\r\n"); 
                        do_tlk = 0;
                    } else {

                        // PET will wait for us here so we have time to fetch our next data byte

                        if (!fetch_next_byte (cur_sa)) {
                            DEBUG_PUTS_P("TLK4 -> TLK5 ERR\r\n"); 
                        }

                        last_byte = !eoi;

                        //_delay_ms(1); // FIXME: debug
                        DEBUG_FLUSH();
                    }
                    goto_state (STATE_TLK5);
                }
                break;

            case STATE_TLK5:
                if (!ieee488_IFC()) {
                    goto_state (STATE_IDLE);
#if 1
                } else if (!ieee488_ATN()) {
                    DEBUG_PUTS_P("TLK5ATN\r\n"); 
                    goto_state (STATE_TLK8);
#endif
                } else if (ieee488_NDAC()) {
                    goto_state (STATE_TLK6);
                }
                break;

            case STATE_TLK6:
                if (!ieee488_IFC()) {
                    goto_state (STATE_IDLE);
#if 1
                } else if (!ieee488_ATN()) {
                    DEBUG_PUTS_P("TLK6ATN\r\n"); 
                    goto_state (STATE_TLK8);
#endif
                } else if (!ieee488_NDAC()) {
                    goto_state (STATE_TLK7);
                }
                break;

            case STATE_TLK7:
                if (!ieee488_IFC()) {
                    goto_state (STATE_IDLE);
                } else if (!ieee488_ATN()) {
                    DEBUG_PUTS_P("TLK7ATN\r\n");
                    goto_state (STATE_TLK8);

                } else {

                    if (do_tlk) { 
                        goto_state (STATE_TLK2);
                    } else {
                        goto_state (STATE_TLK8);
                    }

                }
                break;

            case STATE_TLK8:
                do_tlk = 0;
                if (!ieee488_IFC()) {
                    goto_state (STATE_IDLE);

                } else {
                    goto_state (STATE_LSN1);
                }
                break;
        }

    }
}

void bus_mainloop(void) __attribute__ ((weak, alias("ieee_mainloop_fsm")));
