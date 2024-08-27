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

#include "rpi-aux.h"
#include "rpi-term.h"
#include "rpi-log.h"

void outbyte(char b) {
    RPI_AuxMiniUartWrite(b);
    RPI_TermPutC(b);
}

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
void /*__attribute__((interrupt("UNDEF")))*/ undefined_instruction_vector(void) {
    uint64_t linkRegister;
    asm("mov     %0, lr"    // move the link register into a c variable
        : "=r" (linkRegister));

    asm("push	{r0, r1, r2, r3, r4, r5, ip, lr}");
    RPI_TermPrintRegister(linkRegister);
    //outbyte('U');
    //printf(" LR: %x", linkRegister);
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
void /*__attribute__((interrupt("ABORT")))*/ data_abort_vector(void) {
    uint64_t linkRegister;

    asm("mov     %0, lr"    // move the link register into a c variable
        : "=r" (linkRegister));
    asm("push	{r0, r1, r2, r3, r4, r5, ip, lr}");
    //outbyte('D');
    RPI_TermPrintRegister(linkRegister);
    //printf("LR: %x", linkRegister);
    while (1) {
        LED_ON();
    }
}



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


/* WARNING: ENABLING ANY OF THESE WILL BREAK USB (and probably most interrupt stuff) USE WITH CAUTION & INTENT
  printing text to the screen takes too long & breaks timing stuff, use these only to make sure interrupts are happening properly!
 */
 // enable logging all interrupt events to the screen
#define IRQ_PRINT 0

// enable logging all timer trigger events to the screen
#define TIMER_PRINT 0

// enable the IRQ display on the right edge of the screen
#define IRQ_DISPLAY 0
/* KEY:
 *  ? = checking if this IRQ pending (beginning of for loop)
 *  ! = this IRQ is pending
 *  @ = this IRQ/timer has a handler
 *  # = this IRQ/timer handler was called & returned
 *  - = this timer handler was decremented (or LED jiffy toggler turned off)
 *  { } = prints '{' when entering the interrupt handler, and '}' when the interrupt is finished
 *
 *  COLOR_LIGHTBLUE is IRQ handlers
 *  COLOR_YELLOW is timer handlers
 *  COLOR_BLUE is the timer LED jiffy toggler
 *
 *  Y coordinate is the IRQ/timer number
 */

 /**
     @brief The IRQ Interrupt handler

     This handler is run every time an interrupt source is triggered. It's
     up to the handler to determine the source of the interrupt and most
     importantly clear the interrupt flag so that the interrupt won't
     immediately put us back into the start of the handler again.
 */
void __attribute__((interrupt("IRQ"))) interrupt_vector(void) {

    // LED tracking stuff
    static int lit = 0;
    static int jiffies = 0;

#if IRQ_DISPLAY == 1
    RPI_TermPrintAtDyed(239, IRQ_LINES, COLORS_LIGHTBLUE, COLORS_BLACK, "{");
#endif

    rpi_irq_controller_t* rpiIRQController = (rpi_irq_controller_t*)RPI_INTERRUPT_CONTROLLER_BASE;

    for (int nIRQ = 0; nIRQ < IRQ_LINES; nIRQ++) {

#if IRQ_DISPLAY == 1
        RPI_TermPrintAtDyed(239, nIRQ, COLORS_LIGHTBLUE, COLORS_BLACK, "?");
#endif

        if (ARM_IC_IRQ_PENDING(nIRQ) & ARM_IRQ_MASK(nIRQ)) {
            // this irq is pending
#if IRQ_PRINT == 1
            RPI_TermPrintDyed(COLORS_LIGHTGRAY, COLORS_BLACK, "IRQ %i is pending. ", nIRQ);
#endif

#if IRQ_DISPLAY == 1
            RPI_TermPrintAtDyed(239, nIRQ, COLORS_LIGHTBLUE, COLORS_BLACK, "!");
#endif

            TIRQHandler* pHandler = IRQHandlers[nIRQ];
            if (pHandler) {
#if IRQ_DISPLAY == 1
                RPI_TermPrintAtDyed(239, nIRQ, COLORS_LIGHTBLUE, COLORS_BLACK, "@");
#endif
#if IRQ_PRINT == 1
                RPI_TermPrintDyed(COLORS_LIGHTBLUE, COLORS_BLACK, "IRQ %u using handler %x with param %x", nIRQ, pHandler, IRQParams[nIRQ]);
#endif
                (*pHandler) (IRQParams[nIRQ]);
#if IRQ_DISPLAY == 1
                RPI_TermPrintAtDyed(239, nIRQ, COLORS_LIGHTBLUE, COLORS_BLACK, "#");
#endif
            }

            if (nIRQ == 64) {
#if IRQ_PRINT == 1
                RPI_TermPrintDyed(COLORS_GRAY, COLORS_BLACK, "Using timer handler");
#endif
                RPI_GetArmTimer()->IRQClear = 1;
                // Go through all timers, checking if they have a handler registered
                for (TKernelTimerHandle nTimer = 0; nTimer < TIMER_LINES; nTimer++) {
#if IRQ_DISPLAY == 1
                    RPI_TermPrintAtDyed(238, nTimer, COLORS_YELLOW, COLORS_BLACK, "?");
#endif
                    if (TimerHandlers[nTimer] != 0) {   // If there's a handler
                        if (TimerDelays[nTimer] > 0) {  // If there's still time left,
                            TimerDelays[nTimer] -= 1;   // decrement the time left by 1 (/100 Hz)
#if IRQ_DISPLAY == 1
                            RPI_TermPrintAtDyed(238, nTimer, COLORS_YELLOW, COLORS_BLACK, "-");
#endif

                        } else {    // Otherwise,
#if IRQ_DISPLAY == 1
                            RPI_TermPrintAtDyed(239, nIRQ, COLORS_YELLOW, COLORS_BLACK, "@");
#endif
                            // Call the handler
                            TKernelTimerHandler* pHandler = TimerHandlers[nTimer];
#if TIMER_PRINT == 1
                            RPI_TermPrintDyed(COLORS_YELLOW, COLORS_BLACK, "Timer %u using handler 0x%0X with context 0x%0X...", nTimer, pHandler, TimerContexts[nTimer]);
#endif
                            (*pHandler) (nTimer, TimerParams[nTimer], TimerContexts[nTimer]);
#if TIMER_PRINT == 1
                            RPI_TermPrintDyed(COLORS_YELLOW, COLORS_BLACK, " finished.\n                                                              \n");
#endif
#if IRQ_DISPLAY == 1
                            RPI_TermPrintAtDyed(238, nTimer, COLORS_YELLOW, COLORS_BLACK, "#");
#endif
                            // and remove it afterwards
                            TimerHandlers[nTimer] = 0;
                        }
                    }
                }

#if IRQ_PRINT == 1
                RPI_TermPrintDyed(COLORS_GRAY, COLORS_BLACK, ".");
#endif

                // Flip the LED every 25 timers
                jiffies++;
                if (jiffies >= 25) {
                    jiffies = 0;

                    if (lit) {
                        LED_OFF();
                        lit = 0;
#if IRQ_DISPLAY == 1
                        RPI_TermPrintAtDyed(239, nIRQ, COLORS_BLUE, COLORS_BLACK, "-");
#endif
                    } else {
                        LED_ON();
                        lit = 1;
#if IRQ_DISPLAY == 1
                        RPI_TermPrintAtDyed(239, nIRQ, COLORS_BLUE, COLORS_BLACK, "@");
#endif
                    }
                }
            }
        }
    }
    
#if IRQ_DISPLAY == 1
    RPI_TermPrintAtDyed(239, IRQ_LINES, COLORS_LIGHTBLUE, COLORS_BLACK, "}");
#endif
    
}

static const char fromInt[] = "int";

void ConnectIRQHandler(unsigned nIRQ, TInterruptHandler* pHandler, void* pParam) {
  static rpi_irq_controller_t* rpiIRQController = (rpi_irq_controller_t*)RPI_INTERRUPT_CONTROLLER_BASE; // using RPI_GetIrqController doesn't work

  IRQHandlers[nIRQ] = pHandler;
  IRQParams[nIRQ] = pParam;

  if(nIRQ < ARM_IRQ2_BASE) {
    rpiIRQController->Enable_IRQs_1 = ARM_IRQ_MASK(nIRQ);
  } else {
    if(nIRQ < ARM_IRQBASIC_BASE) {
      rpiIRQController->Enable_IRQs_2 = ARM_IRQ_MASK(nIRQ);
    } else {
      rpiIRQController->Enable_Basic_IRQs = ARM_IRQ_MASK(nIRQ);
    }
  }
}

int ConnectTimerHandler(
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
        RPI_Log(fromInt, LOG_ERROR, "OUT OF TIMER LINES! Timer handler 0x%0X not registered.", pHandler);
        return 0;
    }

    TimerDelays[nTimer] = nHzDelay;
    TimerHandlers[nTimer] = pHandler;
    TimerParams[nTimer] = pParam;
    TimerContexts[nTimer] = pContext;

    return 1;
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
