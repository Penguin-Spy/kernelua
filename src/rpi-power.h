// Power manager (for reboots)

#include "rpi-base.h"

#ifndef RPI_POWER_H
#define RPI_POWER_H

// from USPi env <bcm2835.h>
#define POWER_BASE    (PERIPHERAL_BASE + 0x100000)

#define POWER_RSTC    (POWER_BASE + 0x1C)
#define POWER_RSTC_WRCFG_FULL_RESET 0x20

#define POWER_WDOG    (POWER_BASE + 0x24)

#define POWER_PASSWORD  (0x5A << 24)

// from https://github.com/raspberrypi/linux/blob/3b799b23157f8d3961971c5f956aaf79f0bf1de7/drivers/watchdog/bcm2835_wdt.c
#define POWER_RSTC_WRCFG_CLR		0xffffffcf

#define POWER_RSTS    (POWER_BASE + 0x20)
#define POWER_RSTS_PARTITION_CLR 0xfffffaaa
#define POWER_RSTS_RASPBERRYPI_HALT 0x555

// functions

void RPI_PowerReset();
void RPI_PowerOff();

#endif