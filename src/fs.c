/* fs.c © Penguin_Spy 2023
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.

  Provides high-level functions for reading, writing, and modifying
  files and directories on all attached storage devices.

  This module requests information on the attached storage devices from
  the `storage` module (whose implementation is platform-specific) and
  reads the devices' MBR or GPT parition tables to determine filesystems.
  Then, the appropriate file system module is used to conduct file operations.

  The following directory structure is formed by this module:
    - All files and subdirectories of the "boot" storage device can be found
      under the root directory "/"
    - All files and subdirectories of additional storage devices (USB drives,
      populated floppy disk or CD drives) can be found under the path "/disk/",
      "/disk1/", "/disk2/", etc.

  Note that the paths the Lua environment interacts with are slightly different.
  See the lualib_fs module for how the paths are translated.
*/

#include <stdio.h>
#include <string.h>

#include "rpi-term.h"
#include "rpi-log.h"
#include "rpi-sd.h"
#include "fs_fat.h"


// Initalizes the file system module. Reads from the boot storage device and
//  locates the boot FAT32 partition
int fs_init() {
    // temporarily hardcode reading from the SD card on a Raspberry Pi

    int result = sdInitCard(&printf, &printf, false);

    if(result != SD_OK) {
        RPI_TermPrintDyed(COLORS_ORANGE, COLORS_BLACK, "error during sd init: %i\n", result);
        return result;
    }

    RPI_TermSetTextColor(COLORS_LIME);
    printf("success! reading MBR\n");
    uint8_t buffer[512];
    result = sdTransferBlocks(0, 1, buffer, false);

    if(result != SD_OK) {
        RPI_TermPrintDyed(COLORS_ORANGE, COLORS_BLACK, "error reading MBR: %i\n", result);
        return result;
    }
    printf("success! dumping data:\n");
    RPI_LogDumpColumns("sd", buffer, 512, 16);

    // confirm MBR magic bytes
    if(buffer[0x1FE] != 0x55 || buffer[0x1FF] != 0xAA) {
        RPI_TermPrintDyed(COLORS_ORANGE, COLORS_BLACK, "sector 0 did not have MBR magic bytes!\n");
        return -1;
    }
    // check first partition type (0x0C = FAT32 LBA)
    if(buffer[0x1C2] != 0x0C) {
        RPI_TermPrintDyed(COLORS_ORANGE, COLORS_BLACK, "first partition is not FAT32 LBA!\n");
        return -1;
    }

    // get partition sector start (logical sector)
    uint32_t partition_start_LS = buffer[0x1C6] + (buffer[0x1C7] << 8) + (buffer[0x1C8] << 16) + (buffer[0x1C9] << 24);
    uint32_t partition_size_LS = buffer[0x1CA] + (buffer[0x1CB] << 8) + (buffer[0x1CD] << 16) + (buffer[0x1CE] << 24);
    printf("fat32 partition starting sector, size: %u, %u\n", partition_start_LS, partition_size_LS);

    fs_fat* main_fs = fs_fat_init(partition_start_LS, partition_size_LS);

    return 0;
}
