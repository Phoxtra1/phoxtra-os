#ifndef PHOXTRA_STORAGE_JOURNAL_H
#define PHOXTRA_STORAGE_JOURNAL_H

#include "blockdev.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define STORAGE_JOURNAL_SECTOR_SIZE 512u
#define STORAGE_JOURNAL_SECTOR_COUNT 3u

typedef struct storage_journal {
    block_device_t* device;
    uint64_t journal_start_lba;
    uint64_t target_start_lba;
    uint64_t target_sector_count;
    uint64_t sequence;
    volatile uint32_t busy;
    uint8_t initialized;
    uint8_t recovered;
} storage_journal_t;

/*
 * A single-sector write-ahead journal. Its three consecutive sectors are:
 * metadata copy A, payload data, metadata copy B.
 *
 * target_start_lba/target_sector_count define the ONLY addressable target
 * range and must not overlap the journal sectors. The device must be a
 * registered, explicitly write-enabled SIMULATED block device with working
 * read/write/flush callbacks. Physical disks and read-only devices are refused.
 *
 * Call recover() after init() and before any write. A non-zero recovery result
 * means storage must remain offline/read-only until corruption is investigated.
 */
int storage_journal_init(storage_journal_t* journal, block_device_t* device,
                         uint64_t journal_start_lba,
                         uint64_t target_start_lba,
                         uint64_t target_sector_count);
int storage_journal_recover(storage_journal_t* journal);
int storage_journal_write_sector(storage_journal_t* journal, uint64_t target_lba,
                                 const uint8_t data[STORAGE_JOURNAL_SECTOR_SIZE]);

#ifdef __cplusplus
}
#endif

#endif
