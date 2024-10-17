/* fs_fat.c © Penguin_Spy 2024
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 * This Source Code Form is "Incompatible With Secondary Licenses", as
 * defined by the Mozilla Public License, v. 2.0.
 *
 * The Covered Software may not be used as training or other input data
 * for LLMs, generative AI, or other forms of machine learning or neural
 * networks.

  Functions for reading from/writing to a FAT32 formatted file system.
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
// for file open mode flags
#include <fcntl.h>

#include "log.h"
#include "rpi-sd.h"

#include "fs.h"
#include "fs_fat.h"

#define BYTES_PER_SECTOR 512    // effectively hardcoded in the SD card driver

static const char log_from[] = "fs_fat";

#define FAT32_CLUSTER_ID_MASK       0x0FFFFFFF  // top 4 bits are reserved & not part of the cluster number
#define FAT32_END_OF_CHAIN_MARKERS  0x0FFFFFF8  // 0x0FFFFFF0 - 0x0FFFFFF7 are technically valid clusters to be part of a chain, so we include them.
#define FAT32_END_OF_CHAIN          0x0FFFFFFF  // the marker we write to the FAT to indicate the end of chain

#define FS_FAT_FILEATTR_READONLY    1 << 0
#define FS_FAT_FILEATTR_HIDDEN      1 << 1
#define FS_FAT_FILEATTR_SYSTEM      1 << 2
#define FS_FAT_FILEATTR_VOLUME      1 << 3
#define FS_FAT_FILEATTR_DIRECTORY   1 << 4
#define FS_FAT_FILEATTR_ARCHIVE     1 << 5

static SDRESULT transfer_cluster(fs_fat* self, uint32_t start_cluster, uint32_t cluster_count, uint8_t* buffer, bool write) {
    if(start_cluster < 2 || cluster_count < 1 || buffer == NULL) {
        log_error("transfer cluster invalid parameter %i, %i, %X", start_cluster, cluster_count, buffer);
        return SD_ERROR;
    }
    // convert from FAT32 cluster to logical sector
    uint32_t start_block = self->data_start_LS + ((start_cluster - 2) * self->logical_sectors_per_cluster);
    uint32_t block_count = cluster_count * self->logical_sectors_per_cluster;
    log_notice("transfer cluster: %u,%u : %u,%u, %i", start_cluster, cluster_count, start_block, block_count, write ? 1 : 0);
    // transfer blocks
    return sdTransferBlocks(start_block, block_count, buffer, write);
}

// 512 byte sector / 4 bytes per integer (aka 4 bytes per fat entry)
#define ENTRIES_PER_FAT_SECTOR (BYTES_PER_SECTOR / 4)

static uint32_t find_next_cluster(fs_fat* self, uint32_t from_cluster) {
    // determine index into FAT where the next cluster value is
    // determine which sd card block contains that value
    uint16_t index_in_sector = from_cluster % ENTRIES_PER_FAT_SECTOR;
    uint32_t sector = from_cluster / ENTRIES_PER_FAT_SECTOR;

    // read that block into a buffer
    // TODO: cache this because we are likely to make multiple consecutive reads?
    uint32_t buffer[ENTRIES_PER_FAT_SECTOR];
    int result = sdTransferBlocks(self->fat_start_LS + sector, 1, (uint8_t*)buffer, false);
    if(result != SD_OK) {
        log_error("failed to transfer block in find_next_cluster: %i", result);
        return 0;
    }

    // get the next cluster value
    uint32_t next_cluster = buffer[index_in_sector] & FAT32_CLUSTER_ID_MASK;

    // check if it's an end-of-chain value & return 0 if so,
    if(next_cluster < 2 || next_cluster >= FAT32_END_OF_CHAIN_MARKERS) {
        return 0;
    }
    // or return the value
    return next_cluster;
}

// allocates the next available cluster into the chain that starts at from_cluster
static uint32_t allocate_next_cluster_in_chain(fs_fat* self, uint32_t from_cluster) {
    uint32_t buffer[ENTRIES_PER_FAT_SECTOR];

    // determine index into FAT where the next cluster value is
    // determine which sd card block contains that value
    uint16_t index_in_sector = from_cluster % ENTRIES_PER_FAT_SECTOR;
    uint32_t sector = from_cluster / ENTRIES_PER_FAT_SECTOR;
    uint16_t original_index_in_sector = index_in_sector;
    uint32_t original_sector = sector;

    log_notice("allocating from cluster %i (index %i) in fat sector %i", from_cluster, index_in_sector, sector);

    // read that sector into a buffer
    // TODO: cache this because we are likely to make multiple consecutive reads?
    int result = sdTransferBlocks(self->fat_start_LS + sector, 1, (uint8_t*)buffer, false);
    if(result != SD_OK) {
        log_error("failed to transfer block in allocate_next_cluster_in_chain: %i", result);
        return 0;
    }

    // sanity check that the entry is an end of chain marker
    uint32_t next_cluster = buffer[index_in_sector] & FAT32_CLUSTER_ID_MASK;
    if(next_cluster < FAT32_END_OF_CHAIN_MARKERS) {
        log_error("cannot allocate starting from a non end-of-chain marker! 0x%.8X", next_cluster);
        log_dump_columns(log_from, (uint8_t*)buffer, ENTRIES_PER_FAT_SECTOR, 16);
        return 0;
    }

    goto start_loop; // skip re-reading the first block

    for(sector; sector < self->sectors_per_fat; sector++) {
        // read that sector into a buffer
        int result = sdTransferBlocks(self->fat_start_LS + sector, 1, (uint8_t*)buffer, false);
        if(result != SD_OK) {
            log_error("failed to transfer block in allocate_next_cluster_in_chain (2): %i", result);
            return 0;
        }
        start_loop:

        // start looping through FAT entries until a free cluster is found
        for(index_in_sector; index_in_sector < ENTRIES_PER_FAT_SECTOR; index_in_sector++) {
            log_debug("  i %i e %.8X", index_in_sector, buffer[index_in_sector]);
            if((buffer[index_in_sector] & FAT32_CLUSTER_ID_MASK) == 0) {
                goto found_free_cluster;
            }
        }
        index_in_sector = 0; // check the next FAT sector
    }

    log_notice("didn't find available cluster after file, restarting from beginning of FAT");
    index_in_sector = 2; // first 2 entries (0 & 1) of the FAT aren't real

    // if we didn't find an available cluster after the file, try looking from the start of the FAT
    for(sector = 0; sector < (from_cluster / self->logical_sectors_per_cluster); sector++) {
        // read that sector into a buffer
        int result = sdTransferBlocks(self->fat_start_LS + sector, 1, (uint8_t*)buffer, false);
        if(result != SD_OK) {
            log_error("failed to transfer block in allocate_next_cluster_in_chain (3): %i", result);
            return 0;
        }

        // loop through FAT entries until a free cluster is found
        for(index_in_sector; index_in_sector < ENTRIES_PER_FAT_SECTOR; index_in_sector++) {
            log_debug("  i %i e %.8X", index_in_sector, buffer[index_in_sector]);
            if((buffer[index_in_sector] & FAT32_CLUSTER_ID_MASK) == 0) {
                goto found_free_cluster;
            }
        }
        index_in_sector = 0; // check the next FAT sector
    }
    // failed to find any open clusters
    log_warn("failed to find any available clusters!");
    return 0;

found_free_cluster:;    // weird C syntax requirement (no declaration after label)

    // free cluster found, calcluate which cluster this is
    uint32_t free_cluster = (sector * ENTRIES_PER_FAT_SECTOR) + index_in_sector;
    log_notice("found a free cluster %i at index %i in fat sector %i", free_cluster, index_in_sector, sector);

    // note that if some of these transfers fail, the allocated cluster will still be allocated with nothing pointing to it!

    // write end of chain to this cluster
    buffer[index_in_sector] = FAT32_END_OF_CHAIN;
    log_notice("writing end of chain to index %i in fat sector %i", index_in_sector, sector);

    // skip these read/writes if sector == original_sector
    if(sector != original_sector) {
        log_notice("current sector and original sector differ: %i != %i", sector, original_sector);
        // write FAT data to sector (saving the end of chain)
        result = sdTransferBlocks(self->fat_start_LS + sector, 1, (uint8_t*)buffer, true);
        if(result != SD_OK) {
            log_error("failed to transfer block in allocate_next_cluster_in_chain (4): %i", result);
            return 0;
        }
        // read FAT data from original sector (for modifying the previous end of chain)
        result = sdTransferBlocks(self->fat_start_LS + original_sector, 1, (uint8_t*)buffer, false);
        if(result != SD_OK) {
            log_error("failed to transfer block in allocate_next_cluster_in_chain (5): %i", result);
            return 0;
        }
    }

    // write it to the previous end of chain
    log_notice("writing next cluster %i to index %i in fat sector %i", free_cluster, original_index_in_sector, original_sector);
    buffer[original_index_in_sector] = free_cluster;
    result = sdTransferBlocks(self->fat_start_LS + original_sector, 1, (uint8_t*)buffer, true);
    if(result != SD_OK) {
        log_error("failed to transfer block in allocate_next_cluster_in_chain (6): %i", result);
        return 0;
    }

    // zero out the cluster to remove any data remaining from deleted files
    memset(self->cluster_buffer, 0, self->bytes_per_cluster);
    result = transfer_cluster(self, free_cluster, 1, self->cluster_buffer, true);
    if(result != SD_OK) {
        log_error("failed to zero out cluster %i in allocate_next_cluster_in_chain: %i", free_cluster, result);
        return 0;   // again, allocated cluster becomes orphaned, but it's better than adding unexpected garbage data to a file
    }

    return free_cluster;
}

// just 2 different functions, one for extending a chain, and one for allocating a new file
// only the loop through the fat & loading the next fat block is the same, all the rest is different enough

// allocates the first cluster from the beginning of the partition (for creating a new file)
// returns 0 if no cluster could be allocated
static uint32_t allocate_new_cluster_chain(fs_fat* self) {
    uint32_t buffer[ENTRIES_PER_FAT_SECTOR];

    uint16_t index_in_sector = 2; // first 2 entries (0 & 1) of the FAT aren't real
    // sector is block of the FAT
    for(uint32_t sector = 0; sector < self->sectors_per_fat; sector++) {
        // read that block into a buffer
        int result = sdTransferBlocks(self->fat_start_LS + sector, 1, (uint8_t*)buffer, false);
        if(result != SD_OK) {
            log_error("failed to transfer block in allocate_new_cluster_chain: %i", result);
            return 0;
        }

        // start looping through FAT entries until a free cluster is found
        for(index_in_sector; index_in_sector < ENTRIES_PER_FAT_SECTOR; index_in_sector++) {
            if((buffer[index_in_sector] & FAT32_CLUSTER_ID_MASK) == 0) {
                // free cluster found, calcluate which cluster this is
                return (sector * ENTRIES_PER_FAT_SECTOR) + index_in_sector;
                // TODO: do we also mark this cluster as allocated here? (write the end-of-chain marker to this cluster's fat entry?)
            }
        }

        // no free cluster found in this block
        // check the next one
        index_in_sector = 0;
    }
    // if all blocks of the FAT exhausted, return fail
    return 0;
}

/** ends the cluster chain, marking any clusters after it as free
 * @param from_cluster  the last cluster to be part of the chain
 * @param delete        if true, `from_cluster` is also freed, else it's marked as the end of chain
 */
static void truncate_cluster_chain(fs_fat* self, uint32_t from_cluster, bool delete) {
    uint32_t buffer[ENTRIES_PER_FAT_SECTOR];

    // find FAT entry for from_cluster
    uint16_t index_in_sector = from_cluster % ENTRIES_PER_FAT_SECTOR;
    uint32_t current_sector = from_cluster / ENTRIES_PER_FAT_SECTOR;

    log_notice("truncating cluster chain starting @%i, index %i sector %i", from_cluster, index_in_sector, current_sector);

    int result = sdTransferBlocks(self->fat_start_LS + current_sector, 1, (uint8_t*)buffer, false);
    if(result != SD_OK) {
        log_error("failed to transfer block in truncate_cluster_chain (1): %i", result);
        return;
    }

    // read the next cluster
    uint32_t next_cluster = buffer[index_in_sector];
    if(next_cluster >= FAT32_END_OF_CHAIN_MARKERS) {
        // already ends here, how convenient!
        log_notice("already at end");
        return;
    }

    // mark the chain as ending here
    buffer[index_in_sector] = delete ? 0 : FAT32_END_OF_CHAIN;

    // loop through the cluster chain, freeing all clusters
    while(next_cluster >= 2 && next_cluster < FAT32_END_OF_CHAIN_MARKERS) {
        // determine index into FAT where the next cluster value is
        // determine which sd card block contains that value
        index_in_sector = next_cluster % ENTRIES_PER_FAT_SECTOR;
        uint32_t next_sector = next_cluster / ENTRIES_PER_FAT_SECTOR;

        // load a new FAT sector if necessary
        if(next_sector != current_sector) {
            // save current changes
            result = sdTransferBlocks(self->fat_start_LS + current_sector, 1, (uint8_t*)buffer, true);
            if(result != SD_OK) {
                log_error("failed to transfer block in truncate_cluster_chain (2): %i", result);
                return;
            }
            // load next sector
            result = sdTransferBlocks(self->fat_start_LS + next_sector, 1, (uint8_t*)buffer, false);
            if(result != SD_OK) {
                log_error("failed to transfer block in truncate_cluster_chain (3): %i", result);
                return;
            }
            log_notice("loaded new sector: %i -> %i", current_sector, next_sector);
            current_sector = next_sector;
        }

        log_notice("freeing cluster @%i, index %i sector %i", next_cluster, index_in_sector, current_sector);
        // get the next cluster value
        next_cluster = buffer[index_in_sector] & FAT32_CLUSTER_ID_MASK;
        // free this cluster
        buffer[index_in_sector] = 0;    // TODO: don't reset the upper 4 bits when allocating/freeing(?) clusters
    }

    // save final changes to the FAT
    result = sdTransferBlocks(self->fat_start_LS + current_sector, 1, (uint8_t*)buffer, true);
    if(result != SD_OK) {
        log_error("failed to transfer block in truncate_cluster_chain (4): %i", result);
        return;
    }

    log_notice("finished truncating cluster chain");
    return;
}

static directory_entry* find_directory_item(fs_fat* self, char* remaining_path, uint32_t* current_cluster, uint32_t* entry_index);
static int ensure_correct_cluster(fs_file* file, bool allow_allocating);

// initalizes a FAT32 filesystem when passed the starting logical sector and sector count
fs_fat* fs_fat_init(uint32_t partition_start_LS, uint32_t partition_size_LS) {
    log_notice("mounting fat32 filesystem @%i, #%i", partition_start_LS, partition_size_LS);
    fs_fat* self = malloc(sizeof *self);

    self->partition_start_LS = partition_start_LS;
    self->partition_size_LS = partition_size_LS;

    uint8_t buffer[512];    // stores 1 sd card block

    // read first sector of partition
    int result = sdTransferBlocks(partition_start_LS, 1, buffer, false);
    if(result != SD_OK) {
        log_error("error reading VBR: %i", result);
        return NULL;
    }

    // read FAT volume boot record
    log_notice("volume OEM name: %.8s", buffer + 0x3);
    log_notice("bytes per sector: %u",  buffer[0x00B] + (buffer[0x00C] << 8)); // we always assume this is 512. should probably error if not true
    if(buffer[0x00B] + (buffer[0x00C] << 8) != BYTES_PER_SECTOR) {
        log_error("cannot read fat32 partition! bytes per sector is not 512");
        return NULL;
    }

    self->logical_sectors_per_cluster =     buffer[0x00D];
    log_notice("sectors per cluster: %u",          self->logical_sectors_per_cluster);
    self->bytes_per_cluster = BYTES_PER_SECTOR * self->logical_sectors_per_cluster;
    uint8_t* cluster_buffer = malloc(self->bytes_per_cluster);
    if(cluster_buffer == NULL) {
        log_error("failed to allocate a cluster buffer of size %i", self->bytes_per_cluster);
    }
    self->cluster_buffer = cluster_buffer;

    uint16_t reserved_sectors =             buffer[0x00E] + (buffer[0x00F] << 8);
    log_notice("reserved sectors & FAT start: %u", reserved_sectors);
    uint8_t fat_count =                     buffer[0x010];
    log_notice("FAT count: %u",                    fat_count);
    log_notice("media descriptor: 0x%X",           buffer[0x015]);
    log_notice("total sectors: %u",                buffer[0x020] + (buffer[0x021] << 8) + (buffer[0x022] << 16) + (buffer[0x023] << 24));
    self->sectors_per_fat =              buffer[0x024] + (buffer[0x025] << 8) + (buffer[0x026] << 16) + (buffer[0x027] << 24);
    log_notice("sectors per fat: %u",              self->sectors_per_fat);
    log_notice("version: %X.%X",                   buffer[0x02B], buffer[0x02A]);
    self->root_dir_start_C =                buffer[0x02C] + (buffer[0x02D] << 8) + (buffer[0x02E] << 16) + (buffer[0x02F] << 24);
    log_notice("root dir start cluster: %u",       self->root_dir_start_C);

    self->fat_start_LS = partition_start_LS + reserved_sectors;
    self->data_start_LS = self->fat_start_LS + (self->sectors_per_fat * fat_count);
    log_notice("fat start LS: %u", self->fat_start_LS);
    log_notice("data start LS: %u", self->data_start_LS);

    return self;
}

// no idea if this is how to do it or if i need this, but don't leak memory by forgetting about the buffer!
int fs_fat_uninit(fs_fat* self) {
    free(self->cluster_buffer);
    free(self);
}

// calculates the min, but watch out! double evaluation :)
#define min(X, Y) (((X) < (Y)) ? (X) : (Y))

/** Opens a file on the given FAT32 filesystem
 * @param self the struct returned by `fs_fat_init()`
 * @param name the name of the file to open
 * @param mode file opening mode, O_* defines from fcntl.h
 * @returns a `fs_file` struct on success, or `NULL` on error and sets `errno`.
 */
fs_file* fs_fat_open(fs_fat* self, const char* name, int mode) {
    // make a temporary copy of name so find_directory_item can modify it
    char* mutable_name = malloc(strlen(name) + 1);
    strcpy(mutable_name, name);
    uint32_t entry_cluster = self->root_dir_start_C;
    uint32_t entry_index = 0;
    directory_entry* entry = find_directory_item(self, mutable_name, &entry_cluster, &entry_index);
    free(mutable_name);

    log_notice("found item at %u, %u", entry_cluster, entry_index);


    /*
    ✔   when opening in truncate mode, don't clear the file's cluster chain until the file is closed
    ✔       but set the size to 0

    ✔   when closing the file, mark all unused clusters as free (but don't wipe them, mostly unnecessary and might be slow), and the last used cluster as end-of-chain
    n/a     if number of clusters is > file size

    ?   when writing the last cluster of a file back to disk, clear the extra bytes (past the end of the file) to 0 (so that old file data is eventually cleaned up)
            notably, DON'T DO THE ABOVE WHEN READING, reading should not modify the disk!
    ~✔  when allocating a new cluster for writing, don't load the data that's in it from disk to buffer,
    ~       but do mark it as the one loaded in the buffer, 0 out the buffer, and mark the buffer as dirty
    ✔       thus when the file is closed (or seeked), the data is overwritten

        also directory entry stuff for creating files
    ✔   & updating stored size & whatnot when closing a (modified) file
        also reading directory lists longer than 1 cluster
            oh and writing to them/allocating new clusters to write to a directory list

        also deleting files:
            mark directory entry as deleted
            mark cluster chain as free

        defragmentation? probably just let other OS's handle it
    */



    if(entry == NULL) {
        return NULL;    // find_directory_item always sets errno
    } else if(entry->attr & FS_FAT_FILEATTR_DIRECTORY) {
        errno = EISDIR;
        return NULL;
    }
    fs_file* file = malloc(sizeof *file);
    if(file == NULL) {
        log_error("failed to allocate file struct: %i", errno);
        return NULL;
    }

    file->data.fat.first_cluster_id = (entry->cluster_hi << 16) + entry->cluster_lo;
    file->data.fat.nth_cluster_of_file = 0xFFFFFFFF;    // no cluster is loaded
    file->data.fat.cluster_of_directory_entry = entry_cluster;
    file->data.fat.index_of_directory_entry = entry_index;

    file->filesystem = self;
    file->size = entry->size;
    file->offset = 0;
    file->mode = mode;
    file->buffer = malloc(self->bytes_per_cluster);
    if(file->buffer == NULL) {
        log_error("failed to allocate file buffer: %i", errno);
        free(file);
        return NULL;
    }
    file->buffer_is_modified = false;
    file->file_is_modified = false;

    if(mode & O_TRUNC) {
        if(file->size != 0) {
            file->file_is_modified = true;
        }
        file->size = 0;
    }

    return file;
}

/** Closes an open file, saving its buffer if necessary. */
void fs_fat_close(fs_file* file) {
    fs_fat* self = file->filesystem;
    if(file->buffer_is_modified) {
        // write to the disk in the right cluster (conveniently the currently loaded one)
        int result = transfer_cluster(self, file->data.fat.current_loaded_cluster_id, 1, file->buffer, true);
        if(result != SD_OK) {
            log_error("failed to write cluster of file in fs_fat_close: %i", result);
            // can't really return an error code, since closing still happens. just lose data :(
        }
    }
    // update file size
    // last modified timestamp? i don't think we have a real time clock set up yet
    if(file->file_is_modified) {
        log_notice("file is modified, updating file size of %u, %u", file->data.fat.cluster_of_directory_entry, file->data.fat.index_of_directory_entry);
        // find this file's directory entry
        uint8_t* buffer = self->cluster_buffer;
        transfer_cluster(self, file->data.fat.cluster_of_directory_entry, 1, buffer, false);
        directory_entry* entry = (directory_entry*)&buffer[file->data.fat.index_of_directory_entry * 32]; // directory entries are 32 bytes
        // update size & write back to the disk
        log_notice("  %.8s.%.3s %X @%u, %u bytes", entry->name, entry->ext, entry->attr, (entry->cluster_hi << 16) + entry->cluster_lo, entry->size);
        log_notice("  was %i", entry->size);
        entry->size = file->size;
        log_notice("  now %i", entry->size);
        transfer_cluster(self, file->data.fat.cluster_of_directory_entry, 1, buffer, true);

        // free up any unused clusters after the end of the file
        // seek to the end of the file data, then ensure the cluster chain ends there
        file->offset = file->size;
        if(ensure_correct_cluster(file, false) != 0) {
            log_notice("failed to ensure correct cluster when closing file");
            // make sure we don't end the file too early (file will be saved with extra unused clusters in it's chain)
            return;
        }
        truncate_cluster_chain(self, file->data.fat.current_loaded_cluster_id, false);
    }
}

/** loads the correct cluster for the file's current offset
 * if the buffer is modified, saves it to the disk
 * @param allow_allocating  true if new clusters can be allocated to the file
 * @returns `0` on success, or `-1` on error and sets `errno`.
 */
static int ensure_correct_cluster(fs_file* file, bool allow_allocating) {
    int result;
    fs_fat* filesystem = file->filesystem;

    int nth_cluster_of_offset = file->offset / filesystem->bytes_per_cluster;
    if(file->data.fat.nth_cluster_of_file == nth_cluster_of_offset) {
        log_notice("in correct cluster");
        return 0; // conveniently already in the right cluster :)
    }

    if(file->buffer_is_modified) {
        // write to the disk in the right cluster (conveniently the currently loaded one)
        result = transfer_cluster(filesystem, file->data.fat.current_loaded_cluster_id, 1, file->buffer, true);
        if(result != SD_OK) {
            log_error("failed to write cluster of file in ensure_correct_cluster: %i", result);
            errno = EIO;
            return -1;
        }
        file->buffer_is_modified = false;
    }

    uint32_t current_cluster_id;
    uint32_t current_cluster_id_backup; // second copy for allocation loop
    if(nth_cluster_of_offset < file->data.fat.nth_cluster_of_file) {
        // have to seek backwards, aka from the beginning of the file because fat
        log_notice("seeking from first cluster");
        current_cluster_id = file->data.fat.first_cluster_id;
        file->data.fat.nth_cluster_of_file = 0;
    } else {
        // have to seek forwards
        current_cluster_id = file->data.fat.current_loaded_cluster_id;
        log_notice("seeking forwards from #%i @%i", file->data.fat.nth_cluster_of_file, current_cluster_id);
    }
    current_cluster_id_backup = current_cluster_id;

    while(file->data.fat.nth_cluster_of_file < nth_cluster_of_offset) {
        current_cluster_id = find_next_cluster(filesystem, current_cluster_id);
        log_notice("cluster #%i @%i", file->data.fat.nth_cluster_of_file + 1, current_cluster_id);
        if(current_cluster_id == 0) break;  // reached end of chain
        file->data.fat.nth_cluster_of_file += 1;
    }
    if(file->data.fat.nth_cluster_of_file != nth_cluster_of_offset) {
        if(!allow_allocating) { // reached end of chain without getting to the offset, file ended early
            log_notice("couldn't find cluster");
            errno = EIO;    // not a physical IO error, but broken filesystem data
            return -1;
        }
        log_notice("reached end of chain, allocating clusters");
        // allocate new clusters because we are writing
        current_cluster_id = current_cluster_id_backup;
        while(file->data.fat.nth_cluster_of_file < nth_cluster_of_offset) {
            // find the next free cluster after the current one (& update the chain)
            current_cluster_id = allocate_next_cluster_in_chain(filesystem, current_cluster_id);
            log_notice("allocated cluster #%i @%i", file->data.fat.nth_cluster_of_file + 1, current_cluster_id);
            if(current_cluster_id == 0) break;  // failed to allocate
            file->data.fat.nth_cluster_of_file += 1;
        }
        if(file->data.fat.nth_cluster_of_file != nth_cluster_of_offset) {
            log_warn("couldn't allocate necessary clusters");
            errno = ENOSPC;
            return -1;
        }
    }

    // now we have the cluster id of the data in the file where the offset is pointing
    result = transfer_cluster(filesystem, current_cluster_id, 1, file->buffer, false);
    if(result != SD_OK) {
        log_error("failed to read cluster of file in ensure_correct_cluster: %i", result);
        errno = EIO;
        return -1;
    }
    file->data.fat.current_loaded_cluster_id = current_cluster_id;
    log_notice("read new cluster");
    return 0;
}

/** Reads up to `length` bytes from the file into `read_buffer`.
 * @returns the number of bytes read, `0` for end of file, or `-1` on error and sets `errno`.
 */
int fs_fat_read(fs_file* file, uint8_t* read_buffer, int length) {
    // make sure we have the right cluster loaded
    if(ensure_correct_cluster(file, false) != 0) {
        log_notice("failed to ensure correct cluster");
        return -1;
        // return an error when newlib is repeatedly calling _read() will cause the whole read to fail
        // instead of stopping it early and failing on the next call
    }

    int buffer_offset = file->offset - (file->data.fat.nth_cluster_of_file * file->filesystem->bytes_per_cluster);

    log_notice("buffer offset: %i", buffer_offset);
    if(buffer_offset + length > file->filesystem->bytes_per_cluster) {
        // would read past the end of the buffer
        // TODO: allow reading across multiple clusters (read data from buffer, load next cluster, read remaining data)
        length = file->filesystem->bytes_per_cluster - buffer_offset;   // can never be 0, because then ensure_correct_cluster would've loaded the next cluster
    }
    log_notice("buffer-truncated length: %i", length);
    if(file->offset + length > file->size) {
        // would read past the end of the file
        length = file->size - file->offset;         // shouldn't be negative, because then ensure_correct_cluster would've returned -1
    }
    log_notice("size-truncated length: %i", length);
    if(length < 1) {
        return 0;   // end of file
    }
    memcpy(read_buffer, file->buffer + buffer_offset, length);
    file->offset += length;
    log_notice("read %i bytes, offset now at %i", length, file->offset);
    return length;
}

/** Writes `length` bytes from `buffer` into the file.
 * @returns the number of bytes written, or `-1` on error and sets `errno`.
 */
int fs_fat_write(fs_file* file, uint8_t* write_buffer, int length) {
    // make sure we have the right cluster loaded
    // this extends the cluster chain if necessary & zeroes out allocated clusters
    if(ensure_correct_cluster(file, true) != 0) {
        log_notice("failed to ensure correct cluster");
        return -1;
    }

    int buffer_offset = file->offset - (file->data.fat.nth_cluster_of_file * file->filesystem->bytes_per_cluster);

    log_notice("buffer offset: %i", buffer_offset);
    if(buffer_offset + length > file->filesystem->bytes_per_cluster) {
        // would write past the end of the buffer
        length = file->filesystem->bytes_per_cluster - buffer_offset;   // can never be 0, because then ensure_correct_cluster would've loaded the next cluster
    }
    log_notice("buffer-truncated length: %i", length);

    // have correct cluster loaded, offset into buffer, & length we can safely write

    memcpy(file->buffer + buffer_offset, write_buffer, length);
    file->buffer_is_modified = true;
    file->file_is_modified = true;
    file->offset += length;
    if(file->offset > file->size) {
        file->size = file->offset;  // extend file length
    }
    log_notice("wrote %i bytes, offset now at %i, size now at %i", length, file->offset, file->size);

    return length;
}


// the exact value of the file attirbutes for a long file name entry
#define FS_FAT_LFN_ATTRIBUTES       0x0F//FS_FAT_FILEATTR_VOLUME | FS_FAT_FILEATTR_SYSTEM | FS_FAT_FILEATTR_HIDDEN | FS_FAT_FILEATTR_READONLY
#define FS_FAT_LFN_FIRSTENTRY       1 << 6
// windows NT and later use byte 0x0C of the file entry (directory_entry::reserved) to store the case (if the whole part is all one case)
#define FS_FAT_LFN_LOWERNAME        1 << 3
#define FS_FAT_LFN_LOWEREXTENSION   1 << 4

static uint8_t lfn_buffer[255 * 2]; // 255 USC-2 characters

static directory_entry* find_directory_item(fs_fat* self, char* remaining_path, uint32_t* current_cluster, uint32_t* entry_index) {
    uint8_t* buffer = self->cluster_buffer;

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

    log_notice("token: %s, rempath: %s", token, remaining_path);

    // if the pointers still point to the same location, the seperator wasn't found (we're at the end of the path)
    // or if remaining_path is empty (the last char was a '/')
    bool token_is_last = token == remaining_path || remaining_path[0] == '\0';

    int result = transfer_cluster(self, *current_cluster, 1, buffer, false);
    if(result != SD_OK) {
        log_notice("sd read fail: %u", result);
        errno = EIO;
        return NULL;
    }

    uint8_t* lfn_buffer_pos = lfn_buffer + sizeof(lfn_buffer);
    bool lfn_complete = false; // set to true once the entry with sequence number 0x01 has been read

    for(int entry = 0; entry < self->bytes_per_cluster; entry += 32) {
        if(buffer[entry] == 0x00) {
            log_notice("(end of directory list)");
            errno = ENOENT;
            return NULL;
        } else if(buffer[entry] == 0xE5) { // skip deleted entries
            log_notice("(skipping deleted file)");
            lfn_complete = false;
            continue;
        }

        directory_entry* file = (directory_entry*)&buffer[entry];
        log_notice("  %.8s.%.3s %X @%u, %u bytes", file->name, file->ext, file->attr, (file->cluster_hi << 16) + file->cluster_lo, file->size);

        // this file entry is a long file name entry
        if(file->attr == FS_FAT_LFN_ATTRIBUTES) {
            if(file->name[0] & FS_FAT_LFN_FIRSTENTRY) {
                lfn_buffer_pos = lfn_buffer + sizeof(lfn_buffer); // reset pointer to end of buffer (entries are listed in reverse order)
                memset(lfn_buffer, 0, sizeof(lfn_buffer));
                lfn_complete = false;
            }
            // seek backwards 26 bytes for 13 characters
            lfn_buffer_pos -= 26;
            // copy LFN characters into the buffer (copy all of the entry, even if some of the characters are unused (0xFFFF))
            memcpy(lfn_buffer_pos, &file->name[1], 10); // 10 bytes, 5 USC-2 characters
            memcpy(lfn_buffer_pos + 10, &file->created_time, 12); // 12 bytes, 6 USC-2 characters
            memcpy(lfn_buffer_pos + 22, &file->size, 4); // 4 bytes, 2 USC-2 characters
            // if the sequence number (excluding the first entry bit) is one, the long file name has been completely read
            if((file->name[0] & ~((uint8_t)FS_FAT_LFN_FIRSTENTRY)) == 0x01) {
                lfn_complete = true;
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
                    log_notice("token lfn equal");
                    break;
                } else if(end_of_token || end_of_lfn) { // only one ended, they're not equal
                    log_notice("token or lfn shorter");
                    break;
                }

                if(lfn_buffer_pos[i*2] > 0x7F || lfn_buffer_pos[i*2 + 1] != 0) { // if the current LFN character isn't ASCII, it can't match the path
                    log_notice("lfn not ascii");
                    break;
                }

                char current_lfn_char = lfn_buffer_pos[i*2];// convert lowercase to uppercase
                if(current_lfn_char >= 'a' && current_lfn_char <= 'z') {
                    current_lfn_char -= 0x20;
                }
                if(token[i] != current_lfn_char) {
                    log_notice("token and lfn not equal: %c %c", token[i], current_lfn_char);
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

            log_notice("8.3 name: %.13s", eight_three_name);

            names_equal = (strncmp(token, eight_three_name, 13) == 0);

            log_notice("names equal: %u", names_equal);

            if(!names_equal) {
                continue;
            }
        }

        bool entry_is_dir = file->attr & FS_FAT_FILEATTR_DIRECTORY;

        // if this was the last token, we found the item
        if(token_is_last) {
            // return directory entry
            log_notice("found entry");
            *entry_index = entry / 32;   // return info about where to find this directory entry via the passed pointers
            return file;

        } else if(entry_is_dir) {  // if it wasn't the last token but this entry is a directory
            // recursively traverse the subdirectory
            *current_cluster = (file->cluster_hi << 16) + file->cluster_lo;
            log_notice("travelling recursively with %s and %u", remaining_path, *current_cluster);
            return find_directory_item(self, remaining_path, current_cluster, entry_index);

        } else { // if it wasn't the last token but what we found isn't a directory, the subdir we're looking for doesn't exist (it was a file instead)
            log_notice("found file when needed directory");
            errno = ENOTDIR;
            return NULL;
        }
    }

    // looped through whole first cluster of directory table and didn't find the file OR the end of the directory table
    // TODO: read FAT to find next cluster of this table & loop back around to read it
    /*
        read the FAT for the next cluster of the directory table
        if this was the last one, return not found error
        else read the next cluster and go loop through the table again
    */
    errno = ENOENT;
    return NULL;
}
