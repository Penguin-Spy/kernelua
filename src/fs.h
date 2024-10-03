/* fs.h © Penguin_Spy 2024
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
#ifndef FS_H
#define FS_H

#include <stdbool.h>

typedef struct fs_file fs_file;

#include "fs_fat.h"

struct fs_file {
    int offset;         // the current offset in the file in bytes
    int size;           // the size of the file in bytes
    uint8_t* buffer;    // a buffer to cache data while reading/before writing
    bool buffer_is_modified; // true if the buffer must be written back to the disk before loading other data
    int mode;           // file opening mode, O_* defines from fcntl.h
    fs_fat* filesystem; // the filesystem this file resides on
    union {
        fs_fat_file fat;
    } data;             // filesystem-specific data for accessing this file
};

int fs_init();
int fs_open(const char* name, int mode, int system);
int fs_close(int file_id);
int fs_is_valid_file(int file_id);
int fs_seek(int file_id, int offset, int whence);
int fs_read(int file_id, uint8_t* buffer, int length);
int fs_write(int file_id, uint8_t* buffer, int length);

#endif
