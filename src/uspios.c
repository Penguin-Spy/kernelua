// implementation of functions used in libuspi.a

#include <stdarg.h>
#include <stdio.h>

#include "rpi-systimer.h"
#include "rpi-mailbox-interface.h"
#include "uspios.h"

#include "rpi-term.h"
#include "rpi-interrupts.h"


// Timer
void MsDelay(unsigned nMilliSeconds) {
    RPI_WaitMicroSeconds(nMilliSeconds * 1000); // i had to google this, yes its 🔊
}
void usDelay(unsigned nMicroSeconds) {
    RPI_WaitMicroSeconds(nMicroSeconds);
}

//TODO: KernelTimerHandler????
// what is it, how does it work, what does it want, what do i need to write?

typedef void TKernelTimerHandler(TKernelTimerHandle hTimer, void* pParam, void* pContext);

// returns the timer handle (hTimer)
unsigned StartKernelTimer(
    unsigned nHzDelay,    // in HZ units (see "system configuration" above)
    TKernelTimerHandler* pHandler,
    void* pParam, void* pContext) {	// handed over to the timer handler
    //RPI_TermPrintDyed(COLORS_PINK, COLORS_BLACK, "Starting Kernel timer with delay %uhz, handler 0x%0X, param 0x%0X, & context 0x%0X\n", nHzDelay, pHandler, pParam, pContext);

    // turns out you actually need to call the function to connect the timer handler...
    return ConnectTimerHandler(nHzDelay, pHandler, pParam, pContext);
}

void CancelKernelTimer(unsigned hTimer) {
    RPI_TermPrintDyed(COLORS_BLACK, COLORS_RED, "CancelKernelTimer(%u)\n", hTimer);
    return;
}


// Interrupt handling
//TODO: Interrupt handling???
// what is it, how does it work, what does it want, what do i need to write?
// this stuff?:
/* ARM Timer */
//    RPI_GetArmTimer()->Control = ( RPI_ARMTIMER_CTRL_23BIT |
//            RPI_ARMTIMER_CTRL_ENABLE | RPI_ARMTIMER_CTRL_INT_ENABLE );

    /* Enable the ARM Interrupt controller in the BCM interrupt controller */
//    RPI_EnableARMTimerInterrupt();


typedef void TInterruptHandler(void* pParam);

// USPi uses USB IRQ 9
void ConnectInterrupt(unsigned nIRQ, TInterruptHandler* pHandler, void* pParam) {
    // IRQHandlers[nIRQ] = pHandler
    // IRQParams[nIRQ] = pParam
    // enable the IRQ

    // and then the irq handler does
    /* for(int nIRQ=0; nIRQ < IRQ_LINES; nIRQ++) {
        if(nIRQ is pending) {
            // acknowldege it and then
            IRQHandlers[nIRQ](IRQParams[nIRQ]);
        }
    }*/
    RPI_TermPrintDyed(COLORS_PINK, COLORS_BLACK, "Connecting interrupt #%u with handler 0x%0X & param 0x%0X\n", nIRQ, pHandler, pParam);
    ConnectIRQHandler(nIRQ, pHandler, pParam);
    return;
}


// Property tags (ARM -> VC)

// "set power state" to "on", wait until completed
// returns 0 on failure
int SetPowerStateOn(unsigned nDeviceId) {
    RPI_TermPrintDyed(COLORS_PINK, COLORS_BLACK, "TURNING ON DEVICE %u\n", nDeviceId);

    RPI_PropertyInit(); //                             on, wait
    RPI_PropertyAddTag(TAG_SET_POWER_STATE, nDeviceId, 0x03);
    RPI_PropertyProcess();

    RPI_TermPrintDyed(COLORS_PINK, COLORS_BLACK, "TURNED ON DEVICE!\n");
}

// "get board MAC address"
// returns 0 on failure, 1 on success
int GetMACAddress(unsigned char Buffer[6]) {
    RPI_TermPrintDyed(COLORS_PINK, COLORS_BLACK, "GETTING MAC ADDRESS\n");

    rpi_mailbox_property_t* mp;

    RPI_PropertyInit();
    RPI_PropertyAddTag(TAG_GET_BOARD_MAC_ADDRESS);
    RPI_PropertyProcess();

    mp = RPI_PropertyGet(TAG_GET_BOARD_MAC_ADDRESS);
    Buffer[0] = mp->data.buffer_8[0];
    Buffer[1] = mp->data.buffer_8[1];
    Buffer[2] = mp->data.buffer_8[2];
    Buffer[3] = mp->data.buffer_8[3];
    Buffer[4] = mp->data.buffer_8[4];
    Buffer[5] = mp->data.buffer_8[5];


    RPI_TermPrintDyed(COLORS_PINK, COLORS_BLACK, "GOT MAC ADDRESS\n");
    return 1;
}

// Logging
//#define LOG_ERROR	1
//#define LOG_WARNING	2
//#define LOG_NOTICE	3
//#define LOG_DEBUG	4

void LogWrite(const char* pSource,		// short name of module
    unsigned	   Severity,		// see above
    const char* pMessage, ...) {	// uses printf format options

    int old_color = RPI_TermGetTextColor();

    va_list vl;
    va_start(vl, pMessage);

    switch (Severity) {
        case LOG_ERROR:
            RPI_TermSetTextColor(COLORS_RED);
            break;
        case LOG_WARNING:
            RPI_TermSetTextColor(COLORS_ORANGE);
            break;
        case LOG_DEBUG:
            RPI_TermSetTextColor(COLORS_PURPLE);
            break;
        case LOG_NOTICE:
        default:            // default to white if unknown log level
            RPI_TermSetTextColor(COLORS_WHITE);
            break;
    }
    printf("[%s]: ", pSource);
    vprintf(pMessage, vl);
    printf("\n");

    RPI_TermSetTextColor(old_color);
}

//
// Debug support
//

// display "assertion failed" message and halt
void uspi_assertion_failed(const char* pExpr, const char* pFile, unsigned nLine) {
    /*int old_fg = RPI_TermGetTextColor();
    int old_bg = RPI_TermGetBackgroundColor();

    RPI_TermSetTextColor(COLORS_RED);
    RPI_TermSetBackgroundColor(COLORS_BLACK);
    int x = RPI_TermGetCursorX();
    int y = RPI_TermGetCursorY();*/
    RPI_TermPrintDyed(COLORS_BLACK, COLORS_RED, "<ASSERT_FAIL>: %s, in %s:%i\n", pExpr, pFile, nLine);
    /*RPI_TermSetCursorPos(x, y);

    RPI_TermSetTextColor(old_fg);
    RPI_TermSetBackgroundColor(old_bg);*/

    // oh yeah it said to halt lol
    while (1) {}
}

// display hex dump (pSource can be 0)
void DebugHexdump(const int* pBuffer, unsigned nBufLen, const char* pSource /* = 0 */) {
    if (pSource) {
        RPI_TermPrintDyed(COLORS_PINK, COLORS_BLACK, "[%s]: Hex Dump:\n", pSource);
    }

    for (int i = 0; i < nBufLen, i++;) {
        RPI_TermPrintDyed(COLORS_PINK, COLORS_BLACK, "%0X ", pBuffer[i]);
    }
}