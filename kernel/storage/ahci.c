#include "ahci.h"
#include "partition.h"
#include "../memory/heap.h"
#include "../log/logger.h"

/* Simulated volatile RAM storage; not a physical AHCI/NVMe driver. */
static block_device_t ahci_sata_dev;
static uint8_t* sata_disk_storage = (void*)0;
#define AHCI_DISK_SECTOR_COUNT 65536 // 32MB storage drive

static void kstrncpy(char* dst, const char* src, int maxlen) {
    int i;
    for (i = 0; i < maxlen - 1 && src[i]; i++) {
        dst[i] = src[i];
    }
    dst[i] = '\0';
}

int ahci_read_sectors(block_device_t* dev, uint64_t lba, uint32_t count, uint8_t* buffer) {
    if (dev != &ahci_sata_dev || !sata_disk_storage || !buffer || count == 0u)
        return -1;
    if (lba >= dev->total_sectors || count > dev->total_sectors - lba)
        return -1;

    const uint64_t offset = lba * dev->sector_size;
    const uint64_t bytes = (uint64_t)count * dev->sector_size;
    for (uint64_t i = 0; i < bytes; ++i)
        buffer[i] = sata_disk_storage[offset + i];
    return 0;
}

int ahci_write_sectors(block_device_t* dev, uint64_t lba, uint32_t count, const uint8_t* buffer) {
    /* Physical and simulated persistent writes are not available in this prototype. */
    (void)dev;
    (void)lba;
    (void)count;
    (void)buffer;
    return -1;
}

/* Demo-only partition map for the heap-backed RAM disk below.
 * This must never be used by a physical storage controller driver. */
static void seed_demo_partition_table(void) {
    if (!sata_disk_storage) return;
    mbr_sector_t* mbr = (mbr_sector_t*)sata_disk_storage;
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

    sata_disk_storage = (uint8_t*)kmalloc(AHCI_DISK_SECTOR_COUNT * 512);
    if (!sata_disk_storage) {
        klog(LOG_ERROR, "AHCI: Failed to allocate SATA storage buffer");
        return;
    }

    for (uint64_t i = 0; i < AHCI_DISK_SECTOR_COUNT * 512; i++) {
        sata_disk_storage[i] = 0;
    }

    /* Demo metadata for RAM simulation only; not a disk-install operation. */
    seed_demo_partition_table();
    kstrncpy(ahci_sata_dev.name, "sata0", sizeof(ahci_sata_dev.name));
    ahci_sata_dev.total_sectors = AHCI_DISK_SECTOR_COUNT;
    ahci_sata_dev.sector_size   = 512;
    ahci_sata_dev.flags         = BLOCKDEV_FLAG_SIMULATED | BLOCKDEV_FLAG_READONLY;
    ahci_sata_dev.read_sectors  = ahci_read_sectors;
    ahci_sata_dev.write_sectors = ahci_write_sectors;
    ahci_sata_dev.flush_sectors = (void*)0;
    ahci_sata_dev.priv_data     = (void*)0;

    blockdev_register(&ahci_sata_dev);
    klog(LOG_INFO, "Simulated block device: /dev/sata0 (32 MiB RAM; not a physical disk)");
}
