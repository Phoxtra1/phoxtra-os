#include "ahci.h"
#include "../memory/heap.h"
#include "../log/logger.h"
#include "partition.h"

/* This is a volatile RAM-backed test device, not an AHCI/NVMe hardware driver. */
static block_device_t ram_test_dev;
static uint8_t* ram_test_storage = (void*)0;
#define RAM_TEST_SECTOR_COUNT 65536u /* 32 MiB volatile test storage */

static void kstrncpy(char* dst, const char* src, int maxlen) {
    int i;
    for (i = 0; i < maxlen - 1 && src[i]; i++) {
        dst[i] = src[i];
    }
    dst[i] = '\0';
}

int ahci_read_sectors(block_device_t* dev, uint64_t lba, uint32_t count, uint8_t* buffer) {
    if (!dev || !ram_test_storage || !buffer) return -1;
    if (lba > RAM_TEST_SECTOR_COUNT || (uint64_t)count > RAM_TEST_SECTOR_COUNT - lba) return -1;

    const uint64_t offset = lba * 512u;
    const uint64_t bytes = (uint64_t)count * 512u;
    for (uint64_t i = 0; i < bytes; ++i) buffer[i] = ram_test_storage[offset + i];
    return 0;
}

int ahci_write_sectors(block_device_t* dev, uint64_t lba, uint32_t count, const uint8_t* buffer) {
    (void)lba;
    (void)count;
    (void)buffer;
    /* Physical disk writes are not implemented; test media is read-only too. */
    if (!dev || (dev->flags & BLOCKDEV_FLAG_READONLY)) return -1;
    return -1;
}

void ahci_init(void) {
    klog(LOG_INFO, "Storage: initializing volatile RAM test disk; no physical SATA/NVMe I/O");

    ram_test_storage = (uint8_t*)kmalloc((uint64_t)RAM_TEST_SECTOR_COUNT * 512u);
    if (!ram_test_storage) {
        klog(LOG_ERROR, "Storage: failed to allocate RAM test disk");
        return;
    }

    for (uint64_t i = 0; i < (uint64_t)RAM_TEST_SECTOR_COUNT * 512u; ++i) {
        ram_test_storage[i] = 0;
    }

    /* Seed a demo MBR only inside this kmalloc-backed RAM buffer so the
       read-only parser can exercise partition enumeration in QEMU. */
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

    kstrncpy(ram_test_dev.name, "ramdisk0", sizeof(ram_test_dev.name));
    ram_test_dev.total_sectors = RAM_TEST_SECTOR_COUNT;
    ram_test_dev.sector_size = 512u;
    ram_test_dev.flags = BLOCKDEV_FLAG_SIMULATED | BLOCKDEV_FLAG_READONLY;
    ram_test_dev.read_sectors = ahci_read_sectors;
    ram_test_dev.write_sectors = ahci_write_sectors;
    ram_test_dev.priv_data = (void*)0;

    if (blockdev_register(&ram_test_dev) != 0) {
        klog(LOG_ERROR, "Storage: failed to register RAM test disk");
        return;
    }
    klog(LOG_INFO, "Storage: volatile RAM test disk /dev/ramdisk0 registered read-only");
}
