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
#include <stdlib.h>
#include <errno.h>

#include "rpi-term.h"
#include "rpi-log.h"
#include "rpi-sd.h"

#include "fs.h"

#include "fs_fat.h"

static const char fromFS[] = "fs";
#define log(...) RPI_Log(fromFS, __VA_ARGS__)

#define FS_MAX_OPEN_FILES 32
static fs_file* files[FS_MAX_OPEN_FILES] = { 0 };

static fs_fat* main_fs;

// Initalizes the file system module. Reads from the boot storage device and
//  locates the boot FAT32 partition
int fs_init() {
    // temporarily hardcode reading from the SD card on a Raspberry Pi

    int result = sdInitCard(&printf, &printf, false);

    if(result != SD_OK) {
        log(LOG_ERROR, "error during sd init: %i", result);
        return result;
    }

    log(LOG_NOTICE, "success! reading MBR: ");
    uint8_t buffer[512];
    result = sdTransferBlocks(0, 1, buffer, false);

    if(result != SD_OK) {
        log(LOG_ERROR, "error reading MBR: %i", result);
        return result;
    }
    log(LOG_NOTICE, "success!");

    // confirm MBR magic bytes
    if(buffer[0x1FE] != 0x55 || buffer[0x1FF] != 0xAA) {
        log(LOG_ERROR, "sector 0 did not have MBR magic bytes!");
        return -1;
    }
    // check first partition type (0x0C = FAT32 LBA)
    if(buffer[0x1C2] != 0x0C) {
        log(LOG_ERROR, "first partition is not FAT32 LBA!");
        return -1;
    }

    // get partition sector start (logical sector)
    uint32_t partition_start_LS = buffer[0x1C6] + (buffer[0x1C7] << 8) + (buffer[0x1C8] << 16) + (buffer[0x1C9] << 24);
    uint32_t partition_size_LS = buffer[0x1CA] + (buffer[0x1CB] << 8) + (buffer[0x1CD] << 16) + (buffer[0x1CE] << 24);
    log(LOG_NOTICE, "fat32 partition starting sector, size: %u, %u\n", partition_start_LS, partition_size_LS);

    main_fs = fs_fat_init(partition_start_LS, partition_size_LS);

    return 0;
}

/**
 * @param name the file name
 * @param mode C file open mode (`r`|`w`|`a`|`r+`|`w+`)
 * @param system set to `1` to read files from the root of the boot drive, or `0` to read files as the Lua environment
 * @returns an open file id ("handle") on success, or `-1` on error and sets `errno`.
 */
int fs_open(const char* name, const char* mode, int system) {
    int file_id = -1;
    for(int i = 0; i < FS_MAX_OPEN_FILES; i++) {
        if(files[i] == NULL) {
            file_id = i;
            break;
        }
    }
    if(file_id == -1) {
        errno = ENFILE;
        return -1;
    }

    // skip initial slash if present
    if(name[0] == '/') {
        name = name + 1;
    }

    fs_fat* filesystem;

    // determine which drive to read from
    if(strncmp(name, "disk", 4) == 0) {
        // todo: usb drives & stuff
        errno = ENXIO; // or ENODEV ?
        return -1;
    } else {
        // todo: filter file paths based on `system` flag
        filesystem = main_fs;
    }

    fs_file* file = fs_fat_open(filesystem, name);
    if(file == NULL) {
        return -1;  // fs_fat_open sets errno
    }
    files[file_id] = file;

    // debug open file display
    RPI_TermPrintAtDyed(180, 4 + file_id, COLORS_LIGHTBLUE, COLORS_BLACK, "%2i: %-56.56s", file_id, name);

    return file_id;
}

int fs_close(int file_id) {
    if(!fs_is_valid_file(file_id)) {
        errno = EBADF;
        return -1;
    }
    if(files[file_id]->buffer != NULL) {
        free(files[file_id]->buffer);
    }
    free(files[file_id]);
    files[file_id] = NULL;
    RPI_TermPrintAtDyed(180, 4 + file_id, COLORS_BLUE, COLORS_BLACK, "%2i: <closed>", file_id);
    return 0;
}

/** Checks if a file descriptor is valid.
 * @returns `1` if the file descriptor refers to an open file, or `0` otherwise.
 */
int fs_is_valid_file(int file_id) {
    return file_id >= 0 && file_id < FS_MAX_OPEN_FILES && files[file_id] != NULL;
}

/** Checks if a file descriptor is valid.
 * @returns the resulting offset location in bytes, or `-1` on error and sets `errno`.
 */
int fs_seek(int file_id, int offset, int whence) {
    if(!fs_is_valid_file(file_id)) {
        errno = EBADF;
        return -1;
    }

    fs_file* file = files[file_id];
    int new_offset = -1;

    if(whence == SEEK_SET) {
        new_offset = offset;
    } else if(whence == SEEK_CUR) {
        new_offset = file->offset + offset;
    } else if(whence == SEEK_END) {
        new_offset = file->size + offset;
    }
    // if whence isn't any of the above 3, new_offset remains at -1
    // if new_offset is negative of larger than the file size, fail to seek.
    // TODO: allow seeking past the end of the file, which fills the additional space with \0 but only if data is written there
    if(new_offset < 0 || file->size < new_offset) {
        errno = EINVAL;
        return -1;
    }
    file->offset = new_offset;
    return new_offset;
}

/** Reads up to `length` bytes from the file into `buffer`.
 * @returns the number of bytes read, or `-1` on error and sets `errno`.
 */
int fs_read(int file_id, uint8_t* buffer, int length) {
    if(!fs_is_valid_file(file_id)) {
        errno = EBADF;
        return -1;
    }

    // TODO: change which function is used depending on filesystem type
    fs_file* file = files[file_id];
    return fs_fat_read(file, buffer, length);
}
