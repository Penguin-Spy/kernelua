/*
    Part of the Raspberry-Pi Bare Metal Tutorials
    https://www.valvers.com/rpi/bare-metal/
    Copyright (c) 2013-2018, Brian Sidebotham

    This software is licensed under the MIT License.
    Please see the LICENSE file included with this software.

*/

#ifndef RPI_INTERRUPTS_H
#define RPI_INTERRUPTS_H

#include <stdint.h>

#include "rpi-base.h"

#include "uspios.h"

extern volatile int uptime;
extern void RPI_EnableARMTimerInterrupt(void);
void ConnectIRQHandler(unsigned nIRQ, TInterruptHandler* pHandler, void* pParam);
int ConnectTimerHandler(unsigned nHzDelay, TKernelTimerHandler* pHandler, void* pParam, void* pContext);

#endif
