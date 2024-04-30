// implementation of functions used in libuspi.a

#include <stdarg.h>

#include "rpi-systimer.h"
#include "rpi-mailbox-interface.h"
#include "uspios.h"

#include "rpi-interrupts.h"
#include "rpi-log.h"

static const char fromUSPiOS[] = "uspios";


// Timer
void MsDelay(unsigned nMilliSeconds) {
  RPI_WaitMiliseconds(nMilliSeconds);
}
void usDelay(unsigned nMicroSeconds) {
  RPI_WaitMicroseconds(nMicroSeconds);
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
  RPI_Log(fromUSPiOS, LOG_ERROR, "CancelKernelTimer(%u)", hTimer);
  return;
}


// Interrupt handling

typedef void TInterruptHandler(void* pParam);

// USPi uses USB IRQ 9
void ConnectInterrupt(unsigned nIRQ, TInterruptHandler* pHandler, void* pParam) {
  RPI_Log(fromUSPiOS, LOG_KERNEL, "Connecting interrupt #%u with handler 0x%0X & param 0x%0X", nIRQ, pHandler, pParam);
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
  RPI_Log(fromUSPiOS, LOG_KERNEL, "Turned on device #%u", nDeviceId);
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

  RPI_Log(fromUSPiOS, LOG_KERNEL, "Got MAC address");

  return 1;
}

// Logging

void LogWrite(const char* pSource,		// short name of module
  unsigned	   Severity,		// see above
  const char* pMessage, ...) {	// uses printf format options
  va_list vl;
  va_start(vl, pMessage);
  RPI_vLog(pSource, Severity, pMessage, vl);
}

//
// Debug support
//

// display "assertion failed" message and halt
void uspi_assertion_failed(const char* pExpr, const char* pFile, unsigned nLine) {
  RPI_Log("ASSERT_FAIL", LOG_ERROR, "<ASSERT_FAIL>: %s, in %s:%i", pExpr, pFile, nLine);

  // oh yeah it said to halt lol
  while(1) {}
}

// display hex dump (pSource can be 0)
void DebugHexdump(const int* pBuffer, unsigned nBufLen, const char* pSource /* = 0 */) {
  if(pSource == 0) pSource = "?";
  RPI_LogDump(pSource, pBuffer, nBufLen);
}
