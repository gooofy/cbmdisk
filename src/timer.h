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
 * set up two timers:
 *
 * timer0: FCPU/8 for NODISKEMU
 * timer1: 100 Hz for NODISKEMU
 *
 */

#ifndef _TIMER_H
#define _TIMER_H

#include <util/atomic.h>

#define delay_ms(x) _delay_ms(x)
#define delay_us(x) _delay_us(x)

/* Types for unsigned and signed tick values */
typedef uint16_t tick_t;
typedef int16_t stick_t;

/**
 * start_timeout - start a timeout using timer0
 * @usecs: number of microseconds before timeout (maximum 256 for 8MHz clock)
 *
 * This function sets timer 0 so it will time out after the specified number
 * of microseconds. DON'T use a variable as parameter because it would cause
 * run-time floating point calculations (slow and huge).
 */
static inline __attribute__((always_inline)) void start_timeout(uint16_t usecs) {
  TCNT0  = 256 - ((float)F_CPU/8000000.0) * usecs;
  TIFR0 |= _BV(TOV0);
}

/**
 * has_timed_out - returns true if timeout was reached
 *
 * This function returns true if the overflow flag of timer 0 is set which
 * (together with start_timeout and TIMEOUT_US) will happen when the
 * specified time has elapsed.
 */
static inline uint8_t has_timed_out(void) {
  return TIFR0 & _BV(TOV0);
}

/// Global timing variable, 100 ticks per second
/// Use getticks() !
extern volatile tick_t ticks;

/**
 * getticks - return the current system tick count
 *
 * This inline function returns the current system tick count.
 */
static inline tick_t getticks(void) {
  tick_t tmp;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    tmp = ticks;
  }
  return tmp;
}

#define HZ 100

#define MS_TO_TICKS(x) (x/10)

/* Adapted from Linux 2.6 include/linux/jiffies.h:
 *
 *      These inlines deal with timer wrapping correctly. You are
 *      strongly encouraged to use them
 *      1. Because people otherwise forget
 *      2. Because if the timer wrap changes in future you won't have to
 *         alter your driver code.
 *
 * time_after(a,b) returns true if the time a is after time b.
 *
 * Do this with "<0" and ">=0" to only test the sign of the result. A
 * good compiler would generate better code (and a really good compiler
 * wouldn't care). Gcc is currently neither.
 * (">=0" refers to the time_after_eq macro which wasn't copied)
 */
#define time_after(a,b)         \
         ((stick_t)(b) - (stick_t)(a) < 0)
#define time_before(a,b)        time_after(b,a)

void timer_init (void);
 
#endif //_TIMER_H

