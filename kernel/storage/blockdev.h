#ifndef PHOXTRA_BLOCKDEV_H
#define PHOXTRA_BLOCKDEV_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BLOCKDEV_FLAG_READONLY  0x01
#define BLOCKDEV_FLAG_REMOVABLE 0x02
#define BLOCKDEV_FLAG_PARTITION 0x04
#define BLOCKDEV_FLAG_SIMULATED  0x08
/* Explicit opt-in for controlled test backends only. The write wrapper also
 * requires SIMULATED, a non-read-only device, and a successful flush hook. */
#define BLOCKDEV_FLAG_WRITE_ENABLED 0x10

typedef struct block_device {
    char name[32];
    uint64_t total_sectors;
    uint32_t sector_size;
    uint32_t flags;
    
    int (*read_sectors)(struct block_device* dev, uint64_t lba, uint32_t count, uint8_t* buffer);
    int (*write_sectors)(struct block_device* dev, uint64_t lba, uint32_t count, const uint8_t* buffer);
    
    void* priv_data;
} block_device_t;

void blockdev_init(void);
int blockdev_register(block_device_t* dev);
block_device_t* blockdev_get(const char* name);
block_device_t* blockdev_get_by_index(uint32_t index);
uint32_t blockdev_count(void);

/* Safe write API: only explicitly enabled simulated devices may be written.
 * A flush hook is mandatory; raw physical-device writes remain disabled. */
int blockdev_flush(block_device_t* dev);
int blockdev_write_sectors(block_device_t* dev, uint64_t lba,
                           uint32_t count, const uint8_t* buffer);

#ifdef __cplusplus
}
#endif

#endif
