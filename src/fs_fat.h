/* fs_fat.h © Penguin_Spy 2024
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
#ifndef FS_FAT_H
#define FS_FAT_H

#include <stdint.h>

#include "fs.h"

// a suffix of LS means logical sector (hardcoded as 512-bytes)
// a suffix of C means a FAT cluster (size determined by VBR)
typedef struct {
    uint32_t partition_start_LS;    // first sector of the FAT32 partition, as an absolute offset in 512-byte sectors from the beginning of the storage device
    uint32_t partition_size_LS;
    uint32_t fat_start_LS;          // first sector of the first File Allocation Table, absolute offset
    uint32_t data_start_LS;         // first sector of the data region, absolute offset
    uint32_t root_dir_start_C;      // first cluster of the root directory table (clusters begin in the first sector of the data region)
    uint8_t logical_sectors_per_cluster;
    uint32_t sectors_per_fat;
    uint8_t* cluster_buffer;        // a buffer for this filesystem instance
    int bytes_per_cluster;          // the size of the cluster buffer
} fs_fat;

typedef struct {
    uint32_t first_cluster_id;
    uint32_t current_loaded_cluster_id;     // which cluster id is currently loaded (used to get the next one from the FAT)
    uint32_t nth_cluster_of_file;           // which cluster of the file we currently have in the buffer (1st cluster, 23rd cluster, etc.)
    uint32_t cluster_of_directory_entry;    // which cluster this file's directory entry is in (not necessarily the first cluster of the directory table)
    uint32_t index_of_directory_entry;      // the index (of directory entries) into the directory entry cluster
} fs_fat_file;

typedef struct {
    char name[8];               // 0x00-07
    char ext[3];                // 0x08-0A
    uint8_t attr;               // 0x0B
    uint8_t lowercase;          // 0x0C    used to mark case: if bit 4 is set, the extension is all lowercase, if bit 3 the name is all lowercase
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

fs_fat* fs_fat_init(uint32_t partition_start_LS, uint32_t partition_size_LS);
fs_file* fs_fat_open(fs_fat* self, const char* name, int mode);
void fs_fat_close(fs_file* file);
int fs_fat_read(fs_file* file, uint8_t* buffer, int length);
int fs_fat_write(fs_file* file, uint8_t* write_buffer, int length);

#endif
