#ifndef FS_FAT_H
#define FS_FAT_H

#include <stdint.h>

// a suffix of LS means logical sector (hardcoded as 512-bytes)
// a suffix of C means a FAT cluster (size determined by VBR)
typedef struct {
    uint32_t partition_start_LS;    // first sector of the FAT32 partition, as an absolute offset in 512-byte sectors from the beginning of the storage device
    uint32_t partition_size_LS;
    uint32_t fat_start_LS;          // first sector of the first File Allocation Table, absolute offset
    uint32_t data_start_LS;         // first sector of the data region, absolute offset
    uint32_t root_dir_start_C;      // first cluster of the root directory table (clusters begin in the first sector of the data region)
    uint8_t logical_sectors_per_cluster;
} fs_fat;


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

#endif
