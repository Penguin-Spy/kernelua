/*
    Part of the Raspberry-Pi Bare Metal Tutorials
    https://www.valvers.com/rpi/bare-metal/
    Copyright (c) 2013-2018, Brian Sidebotham

    This software is licensed under the MIT License.
    Please see the LICENSE file included with this software.

*/

#include <stdint.h>
#include "rpi-systimer.h"

static rpi_sys_timer_t* rpiSystemTimer = (rpi_sys_timer_t*)RPI_SYSTIMER_BASE;

rpi_sys_timer_t* RPI_GetSystemTimer(void) {
  return rpiSystemTimer;
}

uint64_t RPI_GetTimerTicks(void) {
	uint64_t resVal;
	uint32_t lowCount;
	do {
		resVal = rpiSystemTimer->counter_hi; 						// Read Arm system timer high count
		lowCount = rpiSystemTimer->counter_lo;						// Read Arm system timer low count
	} while(resVal != (uint64_t)rpiSystemTimer->counter_hi);		// Check hi counter hasn't rolled in that time
	resVal = (uint64_t)resVal << 32 | lowCount;						// Join the 32 bit values to a full 64 bit
	return(resVal);													// Return the uint64_t timer tick count
}

uint64_t RPI_TimerTickDifference(uint64_t first, uint64_t second) {
	if(first > second) {											// If timer one is greater than two then timer rolled
		uint64_t td = UINT64_MAX - first + 1;						// Counts left to roll value
		return second + td;											// Add that to new count
	}
	return second - first;											// Return difference between values
}

void RPI_WaitMicroseconds(uint32_t us) {
  volatile uint32_t ts = rpiSystemTimer->counter_lo;

  while((rpiSystemTimer->counter_lo - ts) < us) {
    /* BLANK */
  }
}

void RPI_WaitCycles(unsigned int cycles) {
  if(cycles) while(cycles--) { asm volatile("nop"); }
}
