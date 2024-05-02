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

uint8_t buffer[1024];


typedef struct {
    char name[8];               // 0x00-07
    char ext[3];                // 0x08-0A
    uint8_t attr;               // 0x0B
    uint8_t reserved1;          // 0x0C
    uint8_t created_ms;         // 0x0D
    uint16_t created_time;      // 0x0E-0F
    uint16_t created_date;      // 0x10-11
    uint16_t accessed_date;     // 0x12-13
    uint16_t cluster_hi;        // 0x14-15
    uint16_t modified_time;     // 0x16-17
    uint16_t modified_date;     // 0x18-19
    uint16_t cluster_lo;        // 0x1A-1B
    uint32_t size;              // 0x1C-1F  in bytes
} directory_entry;


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
    uint32_t partition_start_logical_sector = buffer[0x1C6] + (buffer[0x1C7] << 8) + (buffer[0x1C8] << 16) + (buffer[0x1C9] << 24);
    printf("fat32 partition starting sector: %u\n", partition_start_logical_sector);

    // read first sector of partition
    result = sdTransferBlocks(partition_start_logical_sector, 1, buffer, false);
    if(result != SD_OK) {
        RPI_TermPrintDyed(COLORS_ORANGE, COLORS_BLACK, "error reading VBR: %i\n", result);
        return result;
    }

    RPI_TermSetCursorPos(0, 0);
    printf("success! dumping data:\n");
    RPI_LogDumpColumns("sd", buffer, 512, 16);

    // read FAT volume boot record
    printf("volume OEM name: %.8s\n", buffer + 0x3);
    printf("bytes per sector: %u\n", buffer[0x00B] + (buffer[0x00C] << 8)); // we always assume this is 512. should probably error if not true
    printf("sectors per cluster: %u\n", buffer[0x00D]);
    uint16_t reserved_sectors = buffer[0x00E] + (buffer[0x00F] << 8);
    printf("reserved sectors & FAT start: %u\n", reserved_sectors);
    uint8_t fat_count = buffer[0x010];
    printf("FAT count: %u\n", fat_count);
    printf("media descriptor: 0x%X\n", buffer[0x015]);
    printf("total sectors: %u\n", buffer[0x020] + (buffer[0x021] << 8) + (buffer[0x022] << 16) + (buffer[0x023] << 24));
    uint32_t sectors_per_fat = buffer[0x024] + (buffer[0x025] << 8) + (buffer[0x026] << 16) + (buffer[0x027] << 24);
    printf("sectors per fat: %u\n", sectors_per_fat);
    printf("version: %X.%X\n", buffer[0x02B], buffer[0x02A]);
    uint32_t root_dir_start_cluster = buffer[0x02C] + (buffer[0x02D] << 8) + (buffer[0x02E] << 16) + (buffer[0x02F] << 24);
    printf("root dir start cluster: %u\n", root_dir_start_cluster);

    // read root directory
    uint32_t data_start_logical_sector = partition_start_logical_sector + reserved_sectors + (sectors_per_fat * fat_count);
    // this also needs to include   (root_dir_start_cluster-2) * sectors_per_cluster

    result = sdTransferBlocks(data_start_logical_sector, 2, buffer, false);
    if(result != SD_OK) {
        RPI_TermPrintDyed(COLORS_ORANGE, COLORS_BLACK, "error reading root dir: %i\n", result);
        return result;
    }

    // buffer contains the first sector of the root directory

    RPI_TermSetTextColor(COLORS_WHITE);
    printf("reading root dir success! contents:      \n");

    // maximum 16 directory entries per sector (assuming 512 byte sectors)
    for(int entry = 0; entry < 1024; entry += 32) {
        if(buffer[entry + 0x00] == 0x00) {
            printf("  (end of directory list)                \n");
            break;
        }

        directory_entry* file = (directory_entry *) &buffer[entry];
        if(file->attr & 0x08) {
            printf("  (skipping volume label)                \n");
        } else if(file->name[0] == 0xE5) {
            printf("  (skipping deleted file)                \n");
        } else {
            printf("  %.8s.%.3s %X @%u, %u bytes        \n", file->name, file->ext, file->attr, (file->cluster_hi << 16) + file->cluster_lo, file->size);
        }
    }

    return 0;
}
