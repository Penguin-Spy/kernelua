
#include "rpi-power.h"

void write32(uint32_t nAddress, uint32_t nValue) {
  *(volatile uint32_t*)nAddress = nValue;
}

void RPI_PowerReset() {
  //DataMemBarrier();

  write32(POWER_WDOG, POWER_PASSWORD | 0x1);	// set some timeout

  write32(POWER_RSTC, POWER_PASSWORD | POWER_RSTC_WRCFG_FULL_RESET);

  while (1) {} // wait for reset
}