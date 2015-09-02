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
 * set up two timers:
 *
 * timer0: FCPU/8 for NODISKEMU
 * timer1: 100 Hz for NODISKEMU
 * timer3:   1 Hz for networking
 *
 */


#include "config.h"
#include <avr/io.h>
#include <avr/interrupt.h>

#include "timer.h"
#include "tcpip.h"
#include "time.h"

volatile unsigned long eth_time;

volatile tick_t ticks;

void timer_init (void)
{
  // NODISKEMU timers

  /* Count F_CPU/8 in timer 0 */
  TCCR0B = _BV(CS01);

  /* Set up a 100Hz interrupt using timer 1 */
  OCR1A  = F_CPU / 64 / 100 - 1;
  TCNT1  = 0;
  TCCR1A = 0;
  TCCR1B = _BV(WGM12) | _BV(CS10) | _BV(CS11);
  TIMSK1 |= _BV(OCIE1A);

	// networking timer

	TCCR3B |= (1<<WGM32) | (1<<CS30 | 0<<CS31 | 1<<CS32);
	TCNT3 = 0;
	OCR3A = (F_CPU / 1024) - 1;
	TIMSK3 |= (1 << OCIE3A);

	return;
};

// networking timer interrupt
ISR (TIMER3_COMPA_vect)
{
	//tick 1 second
	eth_time++;
	eth.timer = 1;
}

/* The NODISKEMU main timer interrupt */
ISR (TIMER1_COMPA_vect) {

  /* Enable interrupts ASAP again, esp. for ATN */
	sei();

  ticks++;

}

