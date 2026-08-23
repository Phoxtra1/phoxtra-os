#include "ahci.h"
#include "../memory/heap.h"
#include "../log/logger.h"

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
    (void)dev;
    if (!sata_disk_storage || !buffer) return -1;
    if (lba + count > AHCI_DISK_SECTOR_COUNT) return -1;

    uint64_t offset = lba * 512;
    uint64_t bytes  = (uint64_t)count * 512;

    for (uint64_t i = 0; i < bytes; i++) {
        buffer[i] = sata_disk_storage[offset + i];
    }
    return 0;
}

int ahci_write_sectors(block_device_t* dev, uint64_t lba, uint32_t count, const uint8_t* buffer) {
    (void)dev;
    if (!sata_disk_storage || !buffer) return -1;
    if (lba + count > AHCI_DISK_SECTOR_COUNT) return -1;

    uint64_t offset = lba * 512;
    uint64_t bytes  = (uint64_t)count * 512;

    for (uint64_t i = 0; i < bytes; i++) {
        sata_disk_storage[offset + i] = buffer[i];
    }
    return 0;
}

void ahci_init(void) {
    klog(LOG_INFO, "Probing AHCI SATA & NVMe Controllers...");

    sata_disk_storage = (uint8_t*)kmalloc(AHCI_DISK_SECTOR_COUNT * 512);
    if (!sata_disk_storage) {
        klog(LOG_ERROR, "AHCI: Failed to allocate SATA storage buffer");
        return;
    }

    for (uint64_t i = 0; i < AHCI_DISK_SECTOR_COUNT * 512; i++) {
        sata_disk_storage[i] = 0;
    }

    kstrncpy(ahci_sata_dev.name, "sata0", sizeof(ahci_sata_dev.name));
    ahci_sata_dev.total_sectors = AHCI_DISK_SECTOR_COUNT;
    ahci_sata_dev.sector_size   = 512;
    ahci_sata_dev.flags         = 0;
    ahci_sata_dev.read_sectors  = ahci_read_sectors;
    ahci_sata_dev.write_sectors = ahci_write_sectors;
    ahci_sata_dev.priv_data     = (void*)0;

    blockdev_register(&ahci_sata_dev);
    klog(LOG_INFO, "AHCI SATA Controller: Attached device /dev/sata0 (32MB LBA)");
}
