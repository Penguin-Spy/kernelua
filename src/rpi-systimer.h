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
void RPI_WaitMicroSeconds(uint32_t us);
void RPI_WaitMiliSeconds(uint32_t ms);
void RPI_WaitSeconds(uint32_t secs);

#endif
