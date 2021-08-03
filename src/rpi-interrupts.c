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
    void* linkRegister;
    asm("mov     %0, lr"    // move the link register into a c variable
        : "=r" (linkRegister));

    outbyte('U');
    printf(" LR: %x", linkRegister);
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
    while (1) {
        LED_ON();
    }
}


/**
    @brief The Data Abort interrupt handler

    The CPU will start executing this function. Just trap here as a debug
    solution.
*/
void __attribute__((interrupt("ABORT"))) data_abort_vector(void) {
    outbyte('D');
    while (1) {
        LED_ON();
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

static TIRQHandler* IRQHandlers[IRQ_LINES] = { 0 };
static void* IRQParams[IRQ_LINES] = { 0 };

#define TIMER_LINES 8

static unsigned TimerDelays[TIMER_LINES] = { 0 };
static TKernelTimerHandler* TimerHandlers[TIMER_LINES] = { 0 };
static void* TimerParams[TIMER_LINES] = { 0 };
static void* TimerContexts[TIMER_LINES] = { 0 };


#define	EnableInterrupts()	__asm volatile ("cpsie i")
#define	DisableInterrupts()	__asm volatile ("cpsid i")

#define DataSyncBarrier()	__asm volatile ("mcr p15, 0, %0, c7, c10, 4" : : "r" (0) : "memory")
#define DataMemBarrier() 	__asm volatile ("mcr p15, 0, %0, c7, c10, 5" : : "r" (0) : "memory")

void __attribute__((interrupt("IRQ"))) interrupt_vector(void) {
    DisableInterrupts();
    DataMemBarrier();

    static int lit = 0;
    static int jiffies = 0;
    static int x = 237, y;
    static int color;

    x = RPI_TermGetCursorX();
    y = RPI_TermGetCursorY();
    RPI_TermSetCursorPos(239, IRQ_LINES);
    RPI_TermPutC('{');
    RPI_TermSetCursorPos(x, y);

    rpi_irq_controller_t* rpiIRQController = (rpi_irq_controller_t*)RPI_INTERRUPT_CONTROLLER_BASE;

    for (int nIRQ = 0; nIRQ < IRQ_LINES; nIRQ++) {
        x = RPI_TermGetCursorX();
        y = RPI_TermGetCursorY();
        RPI_TermSetCursorPos(239, nIRQ);
        printf("?");
        RPI_TermSetCursorPos(x, y);
        DataMemBarrier();
        if (ARM_IC_IRQ_PENDING(nIRQ) & ARM_IRQ_MASK(nIRQ)) {
            // this irq is pending
            color = RPI_TermGetTextColor();
            RPI_TermSetTextColor(COLORS_LIGHTGRAY);
            printf("IRQ %i is pending. ", nIRQ);
            RPI_TermSetTextColor(color);

            x = RPI_TermGetCursorX();
            y = RPI_TermGetCursorY();
            RPI_TermSetCursorPos(239, nIRQ);
            printf("!");
            RPI_TermSetCursorPos(x, y);

            TIRQHandler* pHandler = IRQHandlers[nIRQ];
            if (pHandler) {
                color = RPI_TermGetTextColor();
                RPI_TermSetTextColor(COLORS_GRAY);
                printf("IRQ %u using handler %x with param %x", nIRQ, pHandler, IRQParams[nIRQ]);
                RPI_TermSetTextColor(color);
                DataMemBarrier();
                DataSyncBarrier();
                (*pHandler) (IRQParams[nIRQ]);
                DataMemBarrier();
            }

            if (nIRQ == 64) {
                RPI_GetArmTimer()->IRQClear = 1;

                // Go through all timers, checking if they have a handler registered
                for (TKernelTimerHandle nTimer = 0; nTimer < TIMER_LINES; nTimer++) {
                    if (TimerHandlers[nTimer] != 0) {   // If there's a handler
                        if (TimerDelays[nTimer] > 0) {  // If there's still time left,
                            TimerDelays[nTimer] -= 1;   // decrement the time left by 1 (/100 Hz)
                        } else {    // Otherwise,
                            // Call the handler
                            TKernelTimerHandler* pHandler = TimerHandlers[nTimer];
                            DataMemBarrier();
                            DataSyncBarrier();
                            (*pHandler) (nTimer, TimerParams[nTimer], TimerContexts[nTimer]);
                            DataMemBarrier();

                            // and remove it afterwards
                            TimerHandlers[nTimer] = 0;
                        }
                    }
                }


                // Flip the LED every 100 timers
                jiffies++;
                if (jiffies == 25) {
                    jiffies = 0;

                    /*color = RPI_TermGetTextColor();
                    RPI_TermSetTextColor(COLORS_YELLOW);*/
                    if (lit) {
                        LED_OFF();
                        lit = 0;
                        x = RPI_TermGetCursorX();
                        y = RPI_TermGetCursorY();
                        RPI_TermSetCursorPos(239, nIRQ);
                        printf("_");
                        RPI_TermSetCursorPos(0, 0);
                        printf("   ");
                        RPI_TermSetCursorPos(x, y);
                    } else {
                        LED_ON();
                        lit = 1;
                        x = RPI_TermGetCursorX();
                        y = RPI_TermGetCursorY();
                        RPI_TermSetCursorPos(239, nIRQ);
                        printf("@");
                        RPI_TermSetCursorPos(x, y);
                    }
                    //RPI_TermSetTextColor(color);
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

    x = RPI_TermGetCursorX();
    y = RPI_TermGetCursorY();
    RPI_TermSetCursorPos(239, IRQ_LINES);
    RPI_TermPutC('}');
    RPI_TermSetCursorPos(x, y);

    DataMemBarrier();
    DataSyncBarrier();
    EnableInterrupts();
}


#define ARM_IC_IRQS_ENABLE(irq)	(  (irq) < ARM_IRQ2_BASE	\
				 ? rpiIRQController->Enable_IRQs_1 & ARM_IRQ_MASK((irq))		\
				 : ((irq) < ARM_IRQBASIC_BASE	\
				   ? rpiIRQController->Enable_IRQs_2 & ARM_IRQ_MASK((irq))	\
				   : rpiIRQController->Enable_Basic_IRQs & ARM_IRQ_MASK((irq))))

void ConnectIRQHandler(unsigned nIRQ, TInterruptHandler* pHandler, void* pParam) {
    rpi_irq_controller_t* rpiIRQController = (rpi_irq_controller_t*)RPI_INTERRUPT_CONTROLLER_BASE;
    printf("enabling irq %u (group ", nIRQ);

    IRQHandlers[nIRQ] = pHandler;
    IRQParams[nIRQ] = pParam;
    DataSyncBarrier();
    DataMemBarrier();


    if (nIRQ < ARM_IRQ2_BASE) {
        rpiIRQController->Enable_IRQs_1 = ARM_IRQ_MASK(nIRQ);
        DataMemBarrier();
        printf("one).\n");
    } else {
        if (nIRQ < ARM_IRQBASIC_BASE) {
            rpiIRQController->Enable_IRQs_2 = ARM_IRQ_MASK(nIRQ);
            DataMemBarrier();
            printf("two).\n");
        } else {
            rpiIRQController->Enable_Basic_IRQs = ARM_IRQ_MASK(nIRQ);
            DataMemBarrier();
            printf("basic).\n");
        }
    }
}

void ConnectTimerHandler(
    unsigned nHzDelay,    // in HZ units (see "system configuration" above)
    TKernelTimerHandler* pHandler,
    void* pParam, void* pContext) {

    // Search for an empty timer line
    int nTimer;
    for (nTimer = 0; nTimer < TIMER_LINES; nTimer++) {
        if (TimerHandlers[nTimer] == 0) {
            break;
        }
    }
    // No empty timer lines
    if (nTimer == TIMER_LINES) {
        printf("OUT OF TIMER LINES UH OH\nTimer handler %x not registered!", pHandler);
        return;
    }

    printf("connecting timer %i with delay %u to call handler %x with param %x and context %x", nTimer, nHzDelay, pHandler, pParam, pContext);
    TimerDelays[nTimer] = nHzDelay;
    TimerHandlers[nTimer] = pHandler;
    TimerParams[nTimer] = pParam;
    TimerContexts[nTimer] = pContext;
    DataMemBarrier();
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
    while (1) {
        LED_ON();
    }
}
