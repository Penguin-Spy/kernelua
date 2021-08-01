// implementation of functions used in libuspi.a

#include <stdarg.h>
#include <stdio.h>

#include "rpi-systimer.h"
#include "rpi-mailbox-interface.h"
#include "uspios.h"

#include "rpi-term.h"

// Timer
void MsDelay (unsigned nMilliSeconds) {
    RPI_WaitMicroSeconds(nMilliSeconds * 1000); // i had to google this, yes its 🔊
}
void usDelay (unsigned nMicroSeconds) {
    RPI_WaitMicroSeconds(nMicroSeconds);
}

//TODO: KernelTimerHandler????
// what is it, how does it work, what does it want, what do i need to write?

// Interrupt handling
//TODO: Interrupt handling???
// what is it, how does it work, what does it want, what do i need to write?
// this stuff?:
/* ARM Timer */
//    RPI_GetArmTimer()->Control = ( RPI_ARMTIMER_CTRL_23BIT |
//            RPI_ARMTIMER_CTRL_ENABLE | RPI_ARMTIMER_CTRL_INT_ENABLE );

    /* Enable the ARM Interrupt controller in the BCM interrupt controller */
//    RPI_EnableARMTimerInterrupt();



// Property tags (ARM -> VC)

// "set power state" to "on", wait until completed
// returns 0 on failure
int SetPowerStateOn (unsigned nDeviceId) {
    RPI_PropertyInit(); //                             on, wait
    RPI_PropertyAddTag(TAG_SET_POWER_STATE, nDeviceId, 0x03);
    RPI_PropertyProcess();
}

// "get board MAC address"
// returns 0 on failure
int GetMACAddress (unsigned char Buffer[6]) {
    rpi_mailbox_property_t *mp;

    RPI_PropertyInit();
    RPI_PropertyAddTag(TAG_GET_BOARD_MAC_ADDRESS);
    RPI_PropertyProcess();

    mp = RPI_PropertyGet(TAG_GET_BOARD_MAC_ADDRESS);

    Buffer[0] = mp->data.buffer_32[0];
    Buffer[1] = mp->data.buffer_32[1];
    Buffer[2] = mp->data.buffer_32[2];
    Buffer[3] = mp->data.buffer_32[3];
    Buffer[4] = mp->data.buffer_32[4];
    Buffer[5] = mp->data.buffer_32[5];
}

// Logging
//#define LOG_ERROR	1
//#define LOG_WARNING	2
//#define LOG_NOTICE	3
//#define LOG_DEBUG	4

void LogWrite (const char *pSource,		// short name of module
	       unsigned	   Severity,		// see above
	       const char *pMessage, ...) {	// uses printf format options
    va_list vl;
    va_start( vl, pMessage );

	switch(Severity) {
	    case LOG_ERROR:
            RPI_TermSetTextColor(COLORS_RED);
            printf("[%s]: ", pSource);
            vprintf(pMessage, vl);
	        break;
	    case LOG_WARNING:
            RPI_TermSetTextColor(COLORS_ORANGE);
            printf("[%s]: ", pSource);
            vprintf(pMessage, vl);
	        break;
	    case LOG_DEBUG:
            RPI_TermSetTextColor(COLORS_PURPLE);
            printf("[%s]: ", pSource);
            vprintf(pMessage, vl);
	        break;
	    case LOG_NOTICE:
	    default:            // default to white if unknown log level
            RPI_TermSetTextColor(COLORS_WHITE);
            printf("[%s]: ", pSource);
            vprintf(pMessage, vl);
	        break;
	}
}

//
// Debug support
//

// display "assertion failed" message and halt
void uspi_assertion_failed (const char *pExpr, const char *pFile, unsigned nLine) {
    RPI_TermSetTextColor(COLORS_RED);
    RPI_TermSetBackgroundColor(COLORS_BLACK);
    printf("<ASSERT_FAIL>: %s, in %s:%i", pExpr, pFile, nLine);
}

// display hex dump (pSource can be 0)
void DebugHexdump (const void *pBuffer, unsigned nBufLen, const char *pSource /* = 0 */) {
    RPI_TermSetTextColor(COLORS_PINK);
    RPI_TermSetBackgroundColor(COLORS_BLACK);
    if(pSource) {
        printf("[%s]: ", pSource);
    }

    printf("%.*s", nBufLen, pBuffer);
}







