#ifndef PHOXTRA_PARTITION_H
#define PHOXTRA_PARTITION_H

#include "blockdev.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct [[gnu::packed]] {
    uint8_t  boot_indicator;
    uint8_t  start_head;
    uint16_t start_sector_cylinder;
    uint8_t  sys_id;
    uint8_t  end_head;
    uint16_t end_sector_cylinder;
    uint32_t start_lba;
    uint32_t total_sectors;
} mbr_partition_entry_t;

typedef struct [[gnu::packed]] {
    uint8_t  bootstrap[446];
    mbr_partition_entry_t entries[4];
    uint16_t signature; // 0xAA55
} mbr_sector_t;

typedef struct [[gnu::packed]] {
    uint64_t signature; // "EFI PART" (0x5452415020494645ULL)
    uint32_t revision;
    uint32_t header_size;
    uint32_t header_crc32;
    uint32_t reserved;
    uint64_t current_lba;
    uint64_t backup_lba;
    uint64_t first_usable_lba;
    uint64_t last_usable_lba;
    uint8_t  disk_guid[16];
    uint64_t partition_entry_lba;
    uint32_t num_partition_entries;
    uint32_t sizeof_partition_entry;
    uint32_t partition_array_crc32;
} gpt_header_t;

typedef struct [[gnu::packed]] {
    uint8_t  type_guid[16];
    uint8_t  unique_guid[16];
    uint64_t starting_lba;
    uint64_t ending_lba;
    uint64_t attributes;
    uint16_t partition_name[36];
} gpt_entry_t;

typedef struct partition_device {
    block_device_t block_dev;
    block_device_t* master_dev;
    uint64_t start_lba;
    uint64_t sector_count;
    uint8_t sys_id;
} partition_device_t;

void partition_init(void);
int partition_parse(block_device_t* master);

#ifdef __cplusplus
}
#endif

#endif
