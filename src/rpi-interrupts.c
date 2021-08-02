/*
    Part of the Raspberry-Pi Bare Metal Tutorials
    https://www.valvers.com/rpi/bare-metal/
    Copyright (c) 2013-2020, Brian Sidebotham

    This software is licensed under the MIT License.
    Please see the LICENSE file included with this software.

*/

#include <stdint.h>
#include <stdio.h>

#include "rpi-armtimer.h"
#include "rpi-base.h"
#include "rpi-gpio.h"
#include "rpi-interrupts.h"
#include "rpi-interrupts-controller.h"

#include "rpi-term.h"
#include "uspios.h"

extern void outbyte(char b);

volatile int uptime = 0;

/**
    @brief The Reset vector interrupt handler

    This can never be called, since an ARM core reset would also reset the
    GPU and therefore cause the GPU to start running code again until
    the ARM is handed control at the end of boot loading
*/
void __attribute__((interrupt("ABORT"))) reset_vector(void) {
    outbyte('R');
    outbyte('\r');
    outbyte('\n');
    while (1) {
        LED_ON();
    }
}

/**
    @brief The undefined instruction interrupt handler

    If an undefined intstruction is encountered, the CPU will start
    executing this function. Just trap here as a debug solution.
*/
void __attribute__((interrupt("UNDEF"))) undefined_instruction_vector(void) {
    outbyte('U');
    outbyte('\r');
    outbyte('\n');
    while (1) {
        LED_ON();
    }
}


/**
    @brief The supervisor call interrupt handler

    The CPU will start executing this function. Just trap here as a debug
    solution.
*/
void __attribute__((interrupt("SWI"))) software_interrupt_vector(void) {
    outbyte('S');
    outbyte('\r');
    outbyte('\n');
    while (1) {
        LED_ON();
    }
}


/**
    @brief The prefetch abort interrupt handler

    The CPU will start executing this function. Just trap here as a debug
    solution.
*/
void __attribute__((interrupt("ABORT"))) prefetch_abort_vector(void) {
    outbyte('P');
    return;
    //while (1) {
    //    LED_ON();
    //}
}


/**
    @brief The Data Abort interrupt handler

    The CPU will start executing this function. Just trap here as a debug
    solution.
*/
void __attribute__((interrupt("ABORT"))) data_abort_vector(void) {
    //LED_ON();
    outbyte('D');
    outbyte('\r');
    outbyte('\n');
    while (1) {
    }
}


/**
    @brief The IRQ Interrupt handler

    This handler is run every time an interrupt source is triggered. It's
    up to the handler to determine the source of the interrupt and most
    importantly clear the interrupt flag so that the interrupt won't
    immediately put us back into the start of the handler again.
*/

#define ARM_IRQS_PER_REG    32

#define ARM_IRQ1_BASE       0
#define ARM_IRQ2_BASE       (ARM_IRQ1_BASE + ARM_IRQS_PER_REG)
#define ARM_IRQBASIC_BASE   (ARM_IRQ2_BASE + ARM_IRQS_PER_REG)

#define IRQ_LINES           (ARM_IRQS_PER_REG * 2 + 8)

#define ARM_IC_IRQ_PENDING(irq)	(  (irq) < ARM_IRQ2_BASE        \
                                 ? rpiIRQController->IRQ_pending_1         \
                                 : ((irq) < ARM_IRQBASIC_BASE   \
                                   ? rpiIRQController->IRQ_pending_2       \
                                   : rpiIRQController->IRQ_basic_pending))

#define ARM_IRQ_MASK(irq)	(1 << ((irq) & (ARM_IRQS_PER_REG-1)))


typedef void TIRQHandler(void* pParam);

TIRQHandler* IRQHandlers[IRQ_LINES];
void* IRQParams[IRQ_LINES];


void __attribute__((interrupt("IRQ"))) interrupt_vector(void) {
    static int lit = 0;
    static int x = 237, y;
    rpi_irq_controller_t* rpiIRQController = (rpi_irq_controller_t*)RPI_INTERRUPT_CONTROLLER_BASE;

    for (int nIRQ = 0; nIRQ < IRQ_LINES; nIRQ++) {
        x = RPI_TermGetCursorX();
        y = RPI_TermGetCursorX();
        RPI_TermSetCursorPos(239, nIRQ);
        printf("?");
        RPI_TermSetCursorPos(x, y);
        if (ARM_IC_IRQ_PENDING(nIRQ) & ARM_IRQ_MASK(nIRQ)) {
            // this irq is pending
            printf("IRQ %i is pending. ", nIRQ);

            x = RPI_TermGetCursorX();
            y = RPI_TermGetCursorX();
            RPI_TermSetCursorPos(239, nIRQ);
            printf("!");
            RPI_TermSetCursorPos(x, y);

            TIRQHandler* pHandler = IRQHandlers[nIRQ];
            if (pHandler) {
                printf("IRQ %u using handler %x with param %x", nIRQ, pHandler, IRQParams[nIRQ]);
                (*pHandler) (IRQParams[nIRQ]);
            }

            RPI_GetArmTimer()->IRQClear = 1;
            if (nIRQ == 64) {
                // Flip the LED
                if (lit) {
                    LED_OFF();
                    lit = 0;
                    x = RPI_TermGetCursorX();
                    y = RPI_TermGetCursorX();
                    RPI_TermSetCursorPos(239, nIRQ);
                    printf(".");
                    RPI_TermSetCursorPos(x, y);
                } else {
                    LED_ON();
                    lit = 1;
                    x = RPI_TermGetCursorX();
                    y = RPI_TermGetCursorX();
                    RPI_TermSetCursorPos(239, nIRQ);
                    printf("@");
                    RPI_TermSetCursorPos(x, y);
                }
            }
        }
    }
    // for all irqs
        // is this one pending
            // acknowledge it, then
            // get it's handler & param & call


    /*static int lit = 0;
    static int jiffies = 0;

    if (RPI_GetArmTimer()->MaskedIRQ) {
        /* Clear the ARM Timer interrupt - it's the only interrupt we have
           enabled, so we want don't have to work out which interrupt source
           caused us to interrupt */
           /*RPI_GetArmTimer()->IRQClear = 1;

           jiffies++;
           if (jiffies == 2) {
               jiffies = 0;
               uptime++;
           }

           // Flip the LED
           if (lit) {
               LED_OFF();
               lit = 0;
               printf(" Off");
           } else {
               LED_ON();
               lit = 1;
               printf(" On");
           }
       }*/
    RPI_TermPutC(0x1C);
}


#define ARM_IC_IRQS_ENABLE(irq)	(  (irq) < ARM_IRQ2_BASE	\
				 ? rpiIRQController->Enable_IRQs_1 & ARM_IRQ_MASK((irq))		\
				 : ((irq) < ARM_IRQBASIC_BASE	\
				   ? rpiIRQController->Enable_IRQs_2 & ARM_IRQ_MASK((irq))	\
				   : rpiIRQController->Enable_Basic_IRQs & ARM_IRQ_MASK((irq))))

void ConnectIRQHandler(unsigned nIRQ, TInterruptHandler* pHandler, void* pParam) {
    rpi_irq_controller_t* rpiIRQController = (rpi_irq_controller_t*)RPI_INTERRUPT_CONTROLLER_BASE;
    printf("enabling irq %u (group ", nIRQ);
    if (nIRQ < ARM_IRQ2_BASE) {
        rpiIRQController->Enable_IRQs_1 = ARM_IRQ_MASK(nIRQ);
        printf("one). ");
    } else {
        if (nIRQ < ARM_IRQBASIC_BASE) {
            rpiIRQController->Enable_IRQs_2 = ARM_IRQ_MASK(nIRQ);
            printf("two). ");
        } else {
            rpiIRQController->Enable_Basic_IRQs = ARM_IRQ_MASK(nIRQ);
            printf("basic). ");
        }
    }

    IRQHandlers[nIRQ] = pHandler;
    IRQParams[nIRQ] = pParam;
}
/**
    @brief The FIQ Interrupt Handler

    The FIQ handler can only be allocated to one interrupt source. The FIQ has
    a full CPU shadow register set. Upon entry to this function the CPU
    switches to the shadow register set so that there is no need to save
    registers before using them in the interrupt.

    In C you can't see the difference between the IRQ and the FIQ interrupt
    handlers except for the FIQ knowing it's source of interrupt as there can
    only be one source, but the prologue and epilogue code is quite different.
    It's much faster on the FIQ interrupt handler.

    The prologue is the code that the compiler inserts at the start of the
    function, if you like, think of the opening curly brace of the function as
    being the prologue code. For the FIQ interrupt handler this is nearly
    empty because the CPU has switched to a fresh set of registers, there's
    nothing we need to save.

    The epilogue is the code that the compiler inserts at the end of the
    function, if you like, think of the closing curly brace of the function as
    being the epilogue code. For the FIQ interrupt handler this is nearly
    empty because the CPU has switched to a fresh set of registers and so has
    not altered the main set of registers.
*/
void __attribute__((interrupt("FIQ"))) fast_interrupt_vector(void) {
    outbyte('F');
    outbyte('\r');
    outbyte('\n');
    while (1) {
        LED_ON();
    }
}
