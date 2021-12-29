
#include "rpi-base.h"

#ifndef RPI_MEMORY_H
#define RPI_MEMORY_H

#define MEMORY_SECTION_NORMAL    0x0040E   // outer and inner write back, no write allocate
#define MEMORY_SECTION_NORMAL_XN 0x0041E   // 	+ execute never
#define MEMORY_SECTION_DEVICE    0x10416   // shared device
#define MEMORY_SECTION_COHERENT  0x10412   // strongly ordered

int RPI_MemoryEnableMMU();

#endif