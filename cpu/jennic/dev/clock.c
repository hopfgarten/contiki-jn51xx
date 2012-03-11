/*
 * Copyright (c) 2006, Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This module keeps track of the local time.
 */

#include <sys/clock.h>
#include <stdbool.h>
#include <AppHardwareApi.h>
#include "hrclock.h"
#include "gdb2.h"

/* cpu clock of JN5139 is 16 mHz */
#define TICKS_TO_USEC  (16)
#define TICK_TIMER_MAX (0x0fffffff)

static bool ticking = false;

void
clock_init()
{
  vAHI_TickTimerInterval(TICK_TIMER_MAX);
  vAHI_TickTimerConfigure(E_AHI_TICK_TIMER_CONT);
  vAHI_TickTimerWrite(0);
  ticking = true;
}

clock_time_t
clock_time(void)
  /* returns the current time in milli-seconds */
{
  if(!ticking) clock_init();
  return clock_hrtime()/(1000);
}

clock_time_t
clock_seconds(void)
{
  if(!ticking) clock_init();
  return clock_hrtime()/(1000*1000ULL);
}

void
clock_delay(unsigned int i)
{
  clock_time_t start;
  if(!ticking) clock_init();
  start = clock_time();
  while(clock_time()-start<i)
    ;
}

/* return time in micro-seconds after this functions has completed. */
hrclock_t
clock_hrtime()
{
  static hrclock_t time = 0;
  time += u32AHI_TickTimerRead()/TICKS_TO_USEC;
  vAHI_TickTimerWrite(4);
  return time;
}


#ifdef JENNIC_CONF_TIMESYNC

#include "uip.h"
#include "ieee802.h"

static uint8_t synced = 0;
static hrclock_t offset = 0;
uint8_t clock_synced()
{
  return synced;
}

hrclock_t clock_synced_hrtime()
{
  return clock_hrtime() + offset;
}

void clock_synchronize()
{
  synced = 1;
  memcpy(&offset, UIP_ICMP6_TIMESTAMP, sizeof(hrclock_t));
  offset = offset + 1800 - ieee_get_last_timestamp();   //offset = master_clock - recved_clock + time_delay
}

#endif

