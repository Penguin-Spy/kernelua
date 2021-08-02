/*
    Part of the Raspberry-Pi Bare Metal Tutorials
    https://www.valvers.com/rpi/bare-metal/
    Copyright (c) 2013-2020, Brian Sidebotham

    This software is licensed under the MIT License.
    Please see the LICENSE file included with this software.

*/

#include <stdint.h>
#include <stdbool.h>

#include "rpi-armtimer.h"
#include "rpi-base.h"
#include "rpi-gpio.h"
#include "rpi-interrupts.h"
#include "rpi-interrupts-controller.h"
#include "gic-400.h"



extern void RPI_EnableGICInterrupts(void);


/** @brief The BCM2835 Interupt controller peripheral at it's base address */
static rpi_irq_controller_t* rpiIRQController =
(rpi_irq_controller_t*)RPI_INTERRUPT_CONTROLLER_BASE;


/**
    @brief Return the IRQ Controller register set
*/
static rpi_irq_controller_t* RPI_GetIrqController(void) {
    return rpiIRQController;
}

void RPI_EnableARMTimerInterrupt(void) {
#ifdef RPI4
    //    RPI_EnableGICInterrupts();
    gic400_init((void*)0xFF840000UL);
#endif
    RPI_GetIrqController()->Enable_Basic_IRQs = RPI_BASIC_ARM_TIMER_IRQ;
}
