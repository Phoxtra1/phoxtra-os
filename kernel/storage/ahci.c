#include "ahci.h"
#include "partition.h"
#include "../memory/heap.h"
#include "../log/logger.h"

/* Simulated volatile RAM storage; this backend never performs physical disk I/O. */
static block_device_t ram_test_dev;
static uint8_t* ram_test_storage = (void*)0;
#define RAM_TEST_SECTOR_COUNT 65536u
#define RAM_TEST_SECTOR_SIZE 512u

static void kstrncpy(char* dst, const char* src, int maxlen) {
    int i;
    for (i = 0; i < maxlen - 1 && src[i]; i++) dst[i] = src[i];
    dst[i] = '\0';
}

int ahci_read_sectors(block_device_t* dev, uint64_t lba,
                      uint32_t count, uint8_t* buffer) {
    if (dev != &ram_test_dev || !ram_test_storage || !buffer || count == 0u)
        return -1;
    if (lba >= dev->total_sectors || count > dev->total_sectors - lba)
        return -1;

    const uint64_t offset = lba * dev->sector_size;
    const uint64_t bytes = (uint64_t)count * dev->sector_size;
    for (uint64_t i = 0; i < bytes; ++i)
        buffer[i] = ram_test_storage[offset + i];
    return 0;
}

/* Persistent and simulated-device writes are deliberately unavailable in this prototype. */
int ahci_write_sectors(block_device_t* dev, uint64_t lba,
                       uint32_t count, const uint8_t* buffer) {
    (void)dev;
    (void)lba;
    (void)count;
    (void)buffer;
    return -1;
}

/* Demo partition metadata is initialized directly in volatile RAM only. */
static void seed_demo_partition_table(void) {
    if (!ram_test_storage) return;
    mbr_sector_t* mbr = (mbr_sector_t*)ram_test_storage;
    mbr->entries[0].sys_id = 0x83;
    mbr->entries[0].start_lba = 2048;
    mbr->entries[0].total_sectors = 16384;
    mbr->entries[1].sys_id = 0x83;
    mbr->entries[1].start_lba = 18432;
    mbr->entries[1].total_sectors = 32768;
    mbr->entries[2].sys_id = 0x0C;
    mbr->entries[2].start_lba = 51200;
    mbr->entries[2].total_sectors = 14336;
    mbr->signature = 0xAA55;
}

void ahci_init(void) {
    klog(LOG_WARNING, "RAM test disk: simulated volatile storage only; physical AHCI/NVMe I/O is not implemented");

    ram_test_storage = (uint8_t*)kmalloc((size_t)RAM_TEST_SECTOR_COUNT * RAM_TEST_SECTOR_SIZE);
    if (!ram_test_storage) {
        klog(LOG_ERROR, "RAM test disk: allocation failed");
        return;
    }

    for (uint64_t i = 0; i < (uint64_t)RAM_TEST_SECTOR_COUNT * RAM_TEST_SECTOR_SIZE; ++i)
        ram_test_storage[i] = 0;
    seed_demo_partition_table();

    kstrncpy(ram_test_dev.name, "sata0", sizeof(ram_test_dev.name));
    ram_test_dev.total_sectors = RAM_TEST_SECTOR_COUNT;
    ram_test_dev.sector_size = RAM_TEST_SECTOR_SIZE;
    ram_test_dev.flags = BLOCKDEV_FLAG_SIMULATED | BLOCKDEV_FLAG_READONLY;
    ram_test_dev.read_sectors = ahci_read_sectors;
    ram_test_dev.write_sectors = ahci_write_sectors;
    ram_test_dev.flush_sectors = (void*)0;
    ram_test_dev.priv_data = (void*)0;

    if (blockdev_register(&ram_test_dev) != 0) {
        klog(LOG_ERROR, "RAM test disk: block-device registration failed");
        return;
    }
    klog(LOG_INFO, "RAM test disk registered read-only: /dev/sata0 (32 MiB volatile RAM)");
}
