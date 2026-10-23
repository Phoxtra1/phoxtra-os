#include "ahci.h"
#include "../memory/heap.h"
#include "../log/logger.h"
#include "partition.h"

/* This is a volatile RAM-backed test device, not an AHCI/NVMe hardware driver. */
static block_device_t ram_test_dev;
static uint8_t* ram_test_storage = (void*)0;
#define RAM_TEST_SECTOR_COUNT 65536u /* 32 MiB volatile test storage */
#define RAM_TEST_SECTOR_SIZE 512u

static void kstrncpy(char* dst, const char* src, int maxlen) {
    if (!dst || !src || maxlen <= 0) return;
    int i = 0;
    for (; i < maxlen - 1 && src[i]; ++i) dst[i] = src[i];
    dst[i] = '\0';
}

int ahci_read_sectors(block_device_t* dev, uint64_t lba, uint32_t count, uint8_t* buffer) {
    if (dev != &ram_test_dev || !ram_test_storage || !buffer || count == 0 ||
        dev->sector_size != RAM_TEST_SECTOR_SIZE || dev->total_sectors != RAM_TEST_SECTOR_COUNT) {
        return -1;
    }
    if (lba >= dev->total_sectors || (uint64_t)count > dev->total_sectors - lba) return -1;

    /* Bounds above also make the following multiplications safe for this fixed device. */
    const uint64_t offset = lba * RAM_TEST_SECTOR_SIZE;
    const uint64_t bytes = (uint64_t)count * RAM_TEST_SECTOR_SIZE;
    for (uint64_t i = 0; i < bytes; ++i) buffer[i] = ram_test_storage[offset + i];
    return 0;
}

int ahci_write_sectors(block_device_t* dev, uint64_t lba, uint32_t count, const uint8_t* buffer) {
    (void)dev;
    (void)lba;
    (void)count;
    (void)buffer;
    /* No storage writes are enabled in this prototype, including to test media. */
    return -1;
}

void ahci_init(void) {
    if (ram_test_storage) {
        klog(LOG_INFO, "Storage: RAM test disk is already initialized");
        return;
    }
    if (blockdev_get("ramdisk0")) {
        klog(LOG_WARNING, "Storage: ramdisk0 name is already registered; refusing to replace another device");
        return;
    }
    ram_test_storage = (uint8_t*)kmalloc((uint64_t)RAM_TEST_SECTOR_COUNT * RAM_TEST_SECTOR_SIZE);
    if (!ram_test_storage) {
        klog(LOG_ERROR, "Storage: failed to allocate RAM test disk");
        return;
    }

    for (uint64_t i = 0; i < (uint64_t)RAM_TEST_SECTOR_COUNT * RAM_TEST_SECTOR_SIZE; ++i) {
        ram_test_storage[i] = 0;
    }

    /* Seed a demo MBR only inside the RAM allocation so read-only discovery is testable. */
    mbr_sector_t* demo_mbr = (mbr_sector_t*)ram_test_storage;
    demo_mbr->signature = 0xAA55u;
    demo_mbr->entries[0].sys_id = 0x83u;
    demo_mbr->entries[0].start_lba = 2048u;
    demo_mbr->entries[0].total_sectors = 16384u;
    demo_mbr->entries[1].sys_id = 0x83u;
    demo_mbr->entries[1].start_lba = 18432u;
    demo_mbr->entries[1].total_sectors = 32768u;
    demo_mbr->entries[2].sys_id = 0x0Cu;
    demo_mbr->entries[2].start_lba = 51200u;
    demo_mbr->entries[2].total_sectors = 14336u;

    kstrncpy(ram_test_dev.name, "ramdisk0", (int)sizeof(ram_test_dev.name));
    ram_test_dev.total_sectors = RAM_TEST_SECTOR_COUNT;
    ram_test_dev.sector_size = RAM_TEST_SECTOR_SIZE;
    ram_test_dev.flags = BLOCKDEV_FLAG_SIMULATED | BLOCKDEV_FLAG_READONLY;
    ram_test_dev.read_sectors = ahci_read_sectors;
    ram_test_dev.write_sectors = ahci_write_sectors;
    ram_test_dev.priv_data = (void*)0;

    if (blockdev_register(&ram_test_dev) != 0) {
        klog(LOG_ERROR, "Storage: failed to register RAM test disk");
        kfree(ram_test_storage);
        ram_test_storage = (void*)0;
        return;
    }
    klog(LOG_INFO, "Storage: volatile RAM test disk /dev/ramdisk0 registered read-only; physical SATA/NVMe driver not implemented");
}
