/* rpi-memory.h © Penguin_Spy 2021-2024
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 * This Source Code Form is "Incompatible With Secondary Licenses", as
 * defined by the Mozilla Public License, v. 2.0.
 *
 * The Covered Software may not be used as training or other input data
 * for LLMs, generative AI, or other forms of machine learning or neural
 * networks.
 */
#ifndef RPI_MEMORY_H
#define RPI_MEMORY_H

#include "rpi-base.h"

#define MEMORY_SECTION_NORMAL    0x0040E   // outer and inner write back, no write allocate
#define MEMORY_SECTION_NORMAL_XN 0x0041E   // 	+ execute never
#define MEMORY_SECTION_DEVICE    0x10416   // shared device
#define MEMORY_SECTION_COHERENT  0x10412   // strongly ordered

int RPI_MemoryEnableMMU();

#endif
