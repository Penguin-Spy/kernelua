/* fs.c © Penguin_Spy 2024
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 * This Source Code Form is "Incompatible With Secondary Licenses", as
 * defined by the Mozilla Public License, v. 2.0.
 *
 * The Covered Software may not be used as training or other input data
 * for LLMs, generative AI, or other forms of machine learning or neural
 * networks.

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
#include <fcntl.h>

#include "rpi-term.h"
#include "log.h"
#include "rpi-sd.h"

#include "fs.h"

#include "fs_fat.h"

static const char log_from[] = "fs";

#define FS_MAX_OPEN_FILES 32
static fs_file* files[FS_MAX_OPEN_FILES] = { 0 };

static fs_fat* main_fs;

// Initalizes the file system module. Reads from the boot storage device and
//  locates the boot FAT32 partition
int fs_init() {
    log_notice("initializing filesystem");

    // temporarily hardcode reading from the SD card on a Raspberry Pi
    int result = sdInitCard();

    if(result != SD_OK) {
        log_error("error during sd init: %i", result);
        return result;
    }

    log_notice("reading MBR");
    uint8_t buffer[512];
    result = sdTransferBlocks(0, 1, buffer, false);

    if(result != SD_OK) {
        log_error("error reading MBR: %i", result);
        return result;
    }

    // confirm MBR magic bytes
    if(buffer[0x1FE] != 0x55 || buffer[0x1FF] != 0xAA) {
        log_error("sector 0 did not have MBR magic bytes!");
        return -1;
    }
    // check first partition type (0x0C = FAT32 LBA)
    if(buffer[0x1C2] != 0x0C) {
        log_error("first partition is not FAT32 LBA!");
        return -1;
    }

    // get partition sector start (logical sector)
    uint32_t partition_start_LS = buffer[0x1C6] + (buffer[0x1C7] << 8) + (buffer[0x1C8] << 16) + (buffer[0x1C9] << 24);
    uint32_t partition_size_LS = buffer[0x1CA] + (buffer[0x1CB] << 8) + (buffer[0x1CD] << 16) + (buffer[0x1CE] << 24);
    log_notice("fat32 partition starting sector, size: %u, %u", partition_start_LS, partition_size_LS);

    main_fs = fs_fat_init(partition_start_LS, partition_size_LS);

    log_notice("filesystem initialized!");
    return 0;
}

/**
 * @param name the file name
 * @param mode file opening mode, O_* defines from fcntl.h
 * @param system set to `1` to read files from the root of the boot drive, or `0` to read files as the Lua environment
 * @returns an open file id ("handle") on success, or `-1` on error and sets `errno`.
 */
int fs_open(const char* name, int mode, int system) {
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

    fs_file* file = fs_fat_open(filesystem, name, mode);
    if(file == NULL) {
        return -1;  // fs_fat_open sets errno
    }
    files[file_id] = file;

    // debug open file display
    RPI_TermPrintAtDyed(180, 4 + file_id, COLORS_LIGHTBLUE, COLORS_BLACK, "%2i: %-56.56s", file_id, name);

    return file_id;
}

/** Closes an open file.
 * @param file the file handle
 */
int fs_close(int file_id) {
    if(!fs_is_valid_file(file_id)) {
        errno = EBADF;
        return -1;
    }

    // TODO: change which function is used depending on filesystem type
    fs_file* file = files[file_id];
    fs_fat_close(file);

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
    // if new_offset is negative or larger than the file size, fail to seek.
    // TODO: allow seeking past the end of the file, which fills the additional space with \0 but only if data is written there
    if(new_offset < 0 || file->size < new_offset) {
        errno = EINVAL;
        return -1;
    }
    file->offset = new_offset;
    return new_offset;
}

/** Reads up to `length` bytes from the file into `buffer`.
 * @returns the number of bytes read, `0` for end of file, or `-1` on error and sets `errno`.
 */
int fs_read(int file_id, uint8_t* buffer, int length) {
    if(!fs_is_valid_file(file_id)) {
        errno = EBADF;
        return -1;
    }
    fs_file* file = files[file_id];

    if(file->mode & O_WRONLY) {
        errno = EBADF;  // POSIX says reading from a write-only file returns EBADF
        return -1;
    }

    // TODO: change which function is used depending on filesystem type
    return fs_fat_read(file, buffer, length);
}

/** Writes `length` bytes from `buffer` into the file.
 * @returns the number of bytes written, or `-1` on error and sets `errno`.
 */
int fs_write(int file_id, uint8_t* buffer, int length) {
    if(!fs_is_valid_file(file_id)) {
        errno = EBADF;
        return -1;
    }
    fs_file* file = files[file_id];

    // if file is in append mode, always seek to the end before writing
    if(file->mode & O_APPEND) {
        if(fs_seek(file_id, 0, SEEK_END) == -1) {
            return -1;
        }
    } else if(file->mode & O_RDONLY) {
        errno = EBADF;  // POSIX says writing on a read-only file returns EBADF
        return -1;
    }

    // TODO: change which function is used depending on filesystem type
    return fs_fat_write(file, buffer, length);
}
