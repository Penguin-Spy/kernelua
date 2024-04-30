/*
    Part of the Raspberry-Pi Bare Metal Tutorials
    https://www.valvers.com/rpi/bare-metal/
    Copyright (c) 2013-2018, Brian Sidebotham

    This software is licensed under the MIT License.
    Please see the LICENSE file included with this software.

*/

#ifndef RPI_SYSTIMER_H
#define RPI_SYSTIMER_H

#include <stdint.h>

#include "rpi-base.h"

#define RPI_SYSTIMER_BASE       ( PERIPHERAL_BASE + 0x3000 )


typedef struct {
  volatile uint32_t control_status;
  volatile uint32_t counter_lo;
  volatile uint32_t counter_hi;
  volatile uint32_t compare0;
  volatile uint32_t compare1;
  volatile uint32_t compare2;
  volatile uint32_t compare3;
} rpi_sys_timer_t;


rpi_sys_timer_t* RPI_GetSystemTimer(void);
uint64_t RPI_GetTimerTicks(void);
uint64_t RPI_TimerTickDifference(uint64_t first, uint64_t second);
void RPI_WaitMicroseconds(uint32_t us);
void RPI_WaitCycles(unsigned int cycles);

#define RPI_WaitMiliseconds(ms)   RPI_WaitMicroseconds(ms * 1000)
#define RPI_WaitSeconds(secs)     RPI_WaitMicroseconds(secs * 1000000)

#endif
