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

void RPI_PowerReset();

#endif