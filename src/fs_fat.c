/* fs_fat.c © Penguin_Spy 2023
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.

  Functions for reading from/writing to a FAT32 formatted file system.
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "rpi-term.h"
#include "rpi-log.h"
#include "rpi-sd.h"

#include "fs_fat.h"

#define ONE_CLUSTER_BUFFER 512 * 16
static uint8_t buffer[ONE_CLUSTER_BUFFER];

SDRESULT fs_fat_transfer_cluster(fs_fat* self, uint32_t start_cluster, uint32_t cluster_count, uint8_t* buffer, bool write) {
    // convert from FAT32 cluster to logical sector
    uint32_t start_block = self->data_start_LS + ((start_cluster - 2) * self->logical_sectors_per_cluster);
    uint32_t block_count = cluster_count * self->logical_sectors_per_cluster;
    printf("transfer cluster: %u,%u : %u,%u\n", start_cluster, cluster_count, start_block, block_count);
    // transfer blocks
    return sdTransferBlocks(start_block, block_count, buffer, write);
}

directory_entry* fs_fat_find_directory_item(fs_fat* self, char* remaining_path, uint32_t current_cluster);

// initalizes a FAT32 filesystem when passed the starting logical sector and sector count
fs_fat* fs_fat_init(uint32_t partition_start_LS, uint32_t partition_size_LS) {
    fs_fat* self = malloc(sizeof *self);

    self->partition_start_LS = partition_start_LS;
    self->partition_size_LS = partition_size_LS;

    // read first sector of partition
    int result = sdTransferBlocks(partition_start_LS, 1, buffer, false);
    if(result != SD_OK) {
        RPI_TermPrintDyed(COLORS_ORANGE, COLORS_BLACK, "error reading VBR: %i\n", result);
        return NULL;
    }

    // read FAT volume boot record
    printf("volume OEM name: %.8s                           \n", buffer + 0x3);
    printf("bytes per sector: %u                            \n", buffer[0x00B] + (buffer[0x00C] << 8)); // we always assume this is 512. should probably error if not true
    self->logical_sectors_per_cluster = buffer[0x00D];
    printf("sectors per cluster: %u                         \n", self->logical_sectors_per_cluster);
    uint16_t reserved_sectors = buffer[0x00E] + (buffer[0x00F] << 8);
    printf("reserved sectors & FAT start: %u                \n", reserved_sectors);
    uint8_t fat_count = buffer[0x010];
    printf("FAT count: %u                                   \n", fat_count);
    printf("media descriptor: 0x%X                          \n", buffer[0x015]);
    printf("total sectors: %u                               \n", buffer[0x020] + (buffer[0x021] << 8) + (buffer[0x022] << 16) + (buffer[0x023] << 24));
    uint32_t sectors_per_fat = buffer[0x024] + (buffer[0x025] << 8) + (buffer[0x026] << 16) + (buffer[0x027] << 24);
    printf("sectors per fat: %u                             \n", sectors_per_fat);
    printf("version: %X.%X                                  \n", buffer[0x02B], buffer[0x02A]);
    self->root_dir_start_C = buffer[0x02C] + (buffer[0x02D] << 8) + (buffer[0x02E] << 16) + (buffer[0x02F] << 24);
    printf("root dir start cluster: %u                      \n", self->root_dir_start_C);

    self->fat_start_LS = partition_start_LS + reserved_sectors;
    self->data_start_LS = self->fat_start_LS + (sectors_per_fat * fat_count);
    printf("fat start LS: %u                                \n", self->fat_start_LS);
    printf("data start LS: %u                               \n", self->data_start_LS);

    int input = 0;
    do {
        input = getchar();
    } while(input != '\n');

    // read root directory
    result = fs_fat_transfer_cluster(self, self->root_dir_start_C, 1, buffer, false);
    if(result != SD_OK) {
        RPI_TermPrintDyed(COLORS_ORANGE, COLORS_BLACK, "error reading root dir: %i\n", result);
        return NULL;
    }

    input = 0;
    do {
        input = getchar();
    } while(input != '\n');

    // buffer contains the first sector of the root directory

    RPI_TermSetTextColor(COLORS_WHITE);
    printf("reading root dir success!\n");

    // file entries are 32 bytes long, we read 1 sector into buffer   entry < (sizeof(buffer) / 32)
    for(int entry = 0; entry < 32; entry++) {
        if(buffer[entry * 32] == 0x00) {
            printf("  (end of directory list)                \n");
            break;
        }

        directory_entry* file = (directory_entry*)&buffer[entry * 32];
        if(file->attr & 0x08) {
            printf("  (skipping volume label)                \n");
        } else if(file->name[0] == 0xE5) {
            printf("  (skipping deleted file)                \n");
        } else {
            printf("  %.8s.%.3s %X @%u, %u bytes        \n", file->name, file->ext, file->attr, (file->cluster_hi << 16) + file->cluster_lo, file->size);
        }
    }

    input = 0;
    do {
        input = getchar();
    } while(input != '\n');

    for(int i = 0; i < 100; i++) {
        RPI_TermPrintAt(0, i, "                                                                                                      ");
    }
    RPI_TermSetCursorPos(0, 0);

    directory_entry* entry = fs_fat_find_directory_item(self, "longer_subdir/two/Longer-Name-Too.toml", self->root_dir_start_C);
    if(entry == NULL) {
        printf("finding file failed\n");
        return self;
    }

    printf("found file:  %.8s.%.3s %X @%u, %u bytes        \n", entry->name, entry->ext, entry->attr, (entry->cluster_hi << 16) + entry->cluster_lo, entry->size);

    fs_fat_transfer_cluster(self, (entry->cluster_hi << 16) + entry->cluster_lo, 1, buffer, false);
    printf("file contents: %s\n", buffer);

    return self;
}



#define FS_FAT_FILEATTR_READONLY    1 << 0
#define FS_FAT_FILEATTR_HIDDEN      1 << 1
#define FS_FAT_FILEATTR_SYSTEM      1 << 2
#define FS_FAT_FILEATTR_VOLUME      1 << 3
#define FS_FAT_FILEATTR_DIRECTORY   1 << 4
#define FS_FAT_FILEATTR_ARCHIVE     1 << 5

// the exact value of the file attirbutes for a long file name entry
#define FS_FAT_LFN_ATTRIBUTES       0x0F//FS_FAT_FILEATTR_VOLUME | FS_FAT_FILEATTR_SYSTEM | FS_FAT_FILEATTR_HIDDEN | FS_FAT_FILEATTR_READONLY
#define FS_FAT_LFN_FIRSTENTRY       1 << 6
// windows NT and later use byte 0x0C of the file entry (directory_entry::reserved) to store the case (if the whole part is all one case)
#define FS_FAT_LFN_LOWERNAME        1 << 3
#define FS_FAT_LFN_LOWEREXTENSION   1 << 4

static uint8_t lfn_buffer[255 * 2]; // 255 USC-2 characters

static void print_usc2(char* string, int length) {
    for(int i = 0; i < length; i++) {
        // output 2 chars per length
        RPI_TermPutC(*string++);
        RPI_TermPutC(*string++);
    }
}

directory_entry* fs_fat_find_directory_item(fs_fat* self, char* remaining_path, uint32_t current_cluster) {
    // the token starts where remaining_path starts
    char* token = remaining_path;
    for(int i = 0; i < strlen(remaining_path); i++) {
        // if the path seperator is found
        if(remaining_path[i] == '/') {
            remaining_path[i] = '\0';   // terminate the token
            remaining_path = &remaining_path[i+1];  // and set remaining_path to point to the rest of the path
        }
    }

    // convert token to uppercase (FAT32 names are case-insensitive)
    for(int i = 0; i < strlen(token); i++) {
        if(token[i] >= 'a' && token[i] <= 'z') {
            token[i] -= 0x20;
        }
    }

    printf("_token: %s, rempath: %s\n", token, remaining_path);

    // if the pointers still point to the same location, the seperator wasn't found (we're at the end of the path)
    // or if remaining_path is empty (the last char was a '/')
    bool token_is_last = token == remaining_path || remaining_path[0] == '\0';

    int result = fs_fat_transfer_cluster(self, current_cluster, 1, buffer, false);
    if(result != SD_OK) {
        printf("_sd read fail: %u\n", result);
        return NULL;
    }

    uint8_t* lfn_buffer_pos = lfn_buffer + sizeof(lfn_buffer);
    bool lfn_complete = false; // set to true once the entry with sequence number 0x01 has been read

    for(int entry = 0; entry < ONE_CLUSTER_BUFFER; entry += 32) {
        if(buffer[entry] == 0x00) {
            printf("_(end of directory list)                \n");
            return NULL;
        } else if(buffer[entry] == 0xE5) { // skip deleted entries
            printf("_(skipping deleted file)                \n");
            lfn_complete = false;
            continue;
        }

        directory_entry* file = (directory_entry*)&buffer[entry];
        printf("  %.8s.%.3s %X @%u, %u bytes        \n", file->name, file->ext, file->attr, (file->cluster_hi << 16) + file->cluster_lo, file->size);

        // this file entry is a long file name entry
        if(file->attr == FS_FAT_LFN_ATTRIBUTES) {
            if(file->name[0] & FS_FAT_LFN_FIRSTENTRY) {
                printf("_start ");
                lfn_buffer_pos = lfn_buffer + sizeof(lfn_buffer); // reset pointer to end of buffer (entries are listed in reverse order)
                memset(lfn_buffer, 0, sizeof(lfn_buffer));
                lfn_complete = false;
            }
            printf("_LFN ");
            // seek backwards 26 bytes for 13 characters
            lfn_buffer_pos -= 26;
            // copy LFN characters into the buffer (copy all of the entry, even if some of the characters are unused (0xFFFF))
            memcpy(lfn_buffer_pos, &file->name[1], 10); // 10 bytes, 5 USC-2 characters
            memcpy(lfn_buffer_pos + 10, &file->created_time, 12); // 12 bytes, 6 USC-2 characters
            memcpy(lfn_buffer_pos + 22, &file->size, 4); // 4 bytes, 2 USC-2 characters
            print_usc2(lfn_buffer_pos, 13);
            RPI_TermPutC('\n');
            // if the sequence number (excluding the first entry bit) is one, the long file name has been completely read
            if((file->name[0] & ~((uint8_t)FS_FAT_LFN_FIRSTENTRY)) == 0x01) {
                lfn_complete = true;
                printf("_lfn complete: ");
                print_usc2(lfn_buffer_pos, (sizeof(lfn_buffer) - ((lfn_buffer_pos - lfn_buffer))) / 2);
                RPI_TermPutC('\n');
                // TODO: save the FAT filename checksum
            }
            // skip any more processing of this entry
            continue;
        } else if(file->attr & FS_FAT_FILEATTR_VOLUME) {
            // skip the actual volume name entry
            lfn_complete = false;
            continue;
        }

        // if we get here, this is a normal file or directory entry (may still be system, hidden, etc.)
        bool names_equal = false;

        if(lfn_complete) { // if we have a complete long file name, check if its checksum is correct and then if it matches the token
            // TODO: checksum

            // compare path token to long file name
            for(int i = 0; i < 255; i++) {
                bool end_of_token = token[i] == '\0';
                bool end_of_lfn = lfn_buffer_pos[i*2] == '\0' && lfn_buffer_pos[i*2 + 1] == '\0';
                if(end_of_token && end_of_lfn) { // we got to the end of both at the same time, they're equal
                    names_equal = true;
                    printf("_token lfn equal\n");
                    break;
                } else if(end_of_token || end_of_lfn) { // only one ended, they're not equal
                    printf("_token or lfn shorter\n");
                    break;
                }

                if(lfn_buffer_pos[i*2] > 0x7F || lfn_buffer_pos[i*2 + 1] != 0) { // if the current LFN character isn't ASCII, it can't match the path
                    printf("_lfn not ascii\n");
                    break;
                }

                char current_lfn_char = lfn_buffer_pos[i*2];// convert lowercase to uppercase
                if(current_lfn_char >= 'a' && current_lfn_char <= 'z') {
                    current_lfn_char -= 0x20;
                }
                if(token[i] != current_lfn_char) {
                    printf("_token and lfn not equal: %c %c\n", token[i], current_lfn_char);
                    break;
                }
            }
            // if the LFN was complete and had the correct checksum, but doesn't match, skip this file's directory entry
            if(!names_equal) {
                lfn_complete = false;
                continue;
            }

        } else { // no complete LFN when reaching this file, compare 8.3 name
            char eight_three_name[13] = { '\0' }; // 8 name, 1 '.', 3 ext, 1 '\0'
            char* name_end = eight_three_name;
            char* ext_end = eight_three_name;

            // copy name not including padding spaces
            for(int i = 0; i < 8; i++) {
                char file_char = file->name[i];
                eight_three_name[i] = file_char;
                if(file_char != ' ') {
                    name_end = &eight_three_name[i];
                }
            }
            name_end++; // after the last real character
            *name_end = '\0'; // max 9th element of eight_three_name array
            // if not a directory, include extention
            if(!(file->attr & FS_FAT_FILEATTR_DIRECTORY)) {
                *name_end = '.'; // replace the null terminator
                char* ext_start = name_end + 1;
                for(int i = 0; i < 3; i++) {
                    char file_char = file->ext[i];
                    ext_start[i] = file_char;
                    if(file_char != ' ') {
                        ext_end = &ext_start[i];
                    }
                }
                ext_end++; // after the last real character
                *ext_end = '\0'; // max 13th element of eight_three_name array
            }

            printf("_8.3 name: %.13s\n", eight_three_name);

            names_equal = (strncmp(token, eight_three_name, 13) == 0);

            printf("_names equal: %u\n", names_equal);

            if(!names_equal) {
                continue;
            }
        }

        //if(strncmp(file->name, token, 8) == 0) { // directory entry name matches
        bool entry_is_dir = file->attr & FS_FAT_FILEATTR_DIRECTORY;

        // if this was the last token, we found the item
        if(token_is_last) {
            // return directory entry
            printf("_found entry\n");
            return file;

        } else if(entry_is_dir) {  // if it wasn't the last token but this entry is a directory
            // recursively traverse the subdirectory
            printf("_travelling recursively with %s and %u\n", remaining_path, (file->cluster_hi << 16) + file->cluster_lo);
            return fs_fat_find_directory_item(self, remaining_path, (file->cluster_hi << 16) + file->cluster_lo);

        } else { // if it wasn't the last token but what we found isn't a directory, the subdir we're looking for doesn't exist (it was a file instead)
            printf("_found file when needed directory\n");
            return NULL;
        }
    //}
    }

    // looped through whole first sector of directory table and didn't find the file OR the end of the directory table
    // TODO: read FAT to find next cluster of this table & loop back around to read it
    /*
        read the FAT for the next sector of the directory table
        if this was the last one, return not found error
        else read the next sector and go loop through the table again
    */
}
