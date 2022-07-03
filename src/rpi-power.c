
#include "rpi-power.h"

uint32_t read32(uint32_t nAddress) {
  return *(volatile uint32_t*)nAddress;
}

void write32(uint32_t nAddress, uint32_t nValue) {
  *(volatile uint32_t*)nAddress = nValue;
}

// Resets the CPU, rebooting the device
void RPI_PowerReset() {

  // set some timeout
  write32(POWER_WDOG, POWER_PASSWORD | 0x1);

  // reset
  int rstc = read32(POWER_RSTC);
  rstc &= POWER_RSTC_WRCFG_CLR;
  rstc |= POWER_PASSWORD | POWER_RSTC_WRCFG_FULL_RESET;
  write32(POWER_RSTC, rstc);

  while(1) {} // wait for reset
}

// FROM https://github.com/raspberrypi/linux/blob/3b799b23157f8d3961971c5f956aaf79f0bf1de7/drivers/watchdog/bcm2835_wdt.c#L164
/*
 * We can't really power off, but if we do the normal reset scheme, and
 * indicate to bootcode.bin not to reboot, then most of the chip will be
 * powered off.
 */
void RPI_PowerOff() {

  // reboot into "halt" partition
  int rsts = read32(POWER_RSTS);
  rsts &= POWER_RSTS_PARTITION_CLR;
  rsts |= POWER_PASSWORD | POWER_RSTS_RASPBERRYPI_HALT;
  write32(POWER_RSTS, rsts);

  // reset
  RPI_PowerReset();
}