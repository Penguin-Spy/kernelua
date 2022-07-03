// implementation of functions used in libuspi.a

#include <stdarg.h>
#include <stdio.h>

#include "rpi-systimer.h"
#include "rpi-mailbox-interface.h"
#include "uspios.h"

#include "rpi-term.h"
#include "rpi-interrupts.h"

static const char fromUSPiOS[] = "uspios";


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

  // turns out you actually need to call the function to connect the timer handler...
  return ConnectTimerHandler(nHzDelay, pHandler, pParam, pContext);
}

void CancelKernelTimer(unsigned hTimer) {
  LogWrite(fromUSPiOS, LOG_ERROR, "CancelKernelTimer(%u)", hTimer);
  return;
}


// Interrupt handling

typedef void TInterruptHandler(void* pParam);

// USPi uses USB IRQ 9
void ConnectInterrupt(unsigned nIRQ, TInterruptHandler* pHandler, void* pParam) {
  LogWrite(fromUSPiOS, LOG_KERNEL, "Connecting interrupt #%u with handler 0x%0X & param 0x%0X", nIRQ, pHandler, pParam);
  ConnectIRQHandler(nIRQ, pHandler, pParam);
  return;
}


// Property tags (ARM -> VC)

// "set power state" to "on", wait until completed
// returns 0 on failure
int SetPowerStateOn(unsigned nDeviceId) {
  RPI_PropertyInit(); //                             on, wait
  RPI_PropertyAddTag(TAG_SET_POWER_STATE, nDeviceId, 0x03);
  RPI_PropertyProcess();
  LogWrite(fromUSPiOS, LOG_KERNEL, "Turned on device #%u", nDeviceId);
}

// "get board MAC address"
// returns 0 on failure, 1 on success
int GetMACAddress(unsigned char Buffer[6]) {
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

  LogWrite(fromUSPiOS, LOG_KERNEL, "Got MAC address");

  return 1;
}

// Logging

void LogWrite(const char* pSource,		// short name of module
  unsigned	   Severity,		// see above
  const char* pMessage, ...) {	// uses printf format options

  int old_fg = RPI_TermGetTextColor();
  int old_bg = RPI_TermGetBackgroundColor();
  RPI_TermSetBackgroundColor(COLORS_BLACK);

  va_list vl;
  va_start(vl, pMessage);

  switch(Severity) {
    case LOG_ERROR:
      RPI_TermSetTextColor(COLORS_RED);
      break;
    case LOG_WARNING:
      RPI_TermSetTextColor(COLORS_ORANGE);
      break;
    case LOG_DEBUG:
      RPI_TermSetTextColor(COLORS_PURPLE);
      break;
    case LOG_KERNEL:
      RPI_TermSetTextColor(COLORS_PINK);
      break;
    case LOG_MMU:
      RPI_TermSetTextColor(COLORS_CYAN);
      break;
    case LOG_NOTICE:
    default:            // default to white if unknown log level
      RPI_TermSetTextColor(COLORS_WHITE);
      break;
  }
  printf("[%s]: ", pSource);
  vprintf(pMessage, vl);
  printf("\n");

  RPI_TermSetTextColor(old_fg);
  RPI_TermSetBackgroundColor(old_bg);
}

//
// Debug support
//

// display "assertion failed" message and halt
void uspi_assertion_failed(const char* pExpr, const char* pFile, unsigned nLine) {
  RPI_TermPrintDyed(COLORS_BLACK, COLORS_RED, "<ASSERT_FAIL>: %s, in %s:%i\n", pExpr, pFile, nLine);

  // oh yeah it said to halt lol
  while(1) {}
}

// display hex dump (pSource can be 0)
void DebugHexdump(const int* pBuffer, unsigned nBufLen, const char* pSource /* = 0 */) {
  if(pSource) {
    RPI_TermPrintDyed(COLORS_PINK, COLORS_BLACK, "[%s]: Dumping %u bytes at 0x%0X:\n", pSource, nBufLen, pBuffer);
  } else {
    RPI_TermPrintDyed(COLORS_PINK, COLORS_BLACK, "[?]: Dumping %u bytes at 0x%0X:\n", nBufLen, pBuffer);
  }

  //int* ptr = pBuffer;

  while(nBufLen-- > 0) {
    printf("%02X ", *pBuffer++);
  }

  /*for(int i = 0; i < nBufLen; i++) {
    RPI_TermPrintDyed(COLORS_PINK, COLORS_BLACK, "%0X ", pBuffer[i]);
  }*/

  //printf("%.*x", nBufLen, pBuffer);

  printf("\n");
}