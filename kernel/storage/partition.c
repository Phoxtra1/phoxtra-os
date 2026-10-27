#include "partition.h"
#include "../memory/heap.h"
#include "../log/logger.h"

static int partition_read_sectors(block_device_t* dev, uint64_t lba, uint32_t count, uint8_t* buffer) {
    if (!dev || !dev->priv_data) return -1;
    partition_device_t* part = (partition_device_t*)dev->priv_data;
    if (lba + count > part->sector_count) return -1;

    return part->master_dev->read_sectors(part->master_dev, part->start_lba + lba, count, buffer);
}

static int partition_write_sectors(block_device_t* dev, uint64_t lba, uint32_t count, const uint8_t* buffer) {
    /* Partition writes remain disabled even if called outside the blockdev wrapper. */
    (void)dev;
    (void)lba;
    (void)count;
    (void)buffer;
    return -1;
}

static void kstrncpy(char* dst, const char* src, int maxlen) {
    int i;
    for (i = 0; i < maxlen - 1 && src[i]; i++) {
        dst[i] = src[i];
    }
    dst[i] = '\0';
}

static void create_partition_subdev(block_device_t* master, const char* name, uint64_t start_lba, uint64_t count, uint8_t sys_id) {
    partition_device_t* part = (partition_device_t*)kmalloc(sizeof(partition_device_t));
    if (!part) return;

    part->master_dev   = master;
    part->start_lba    = start_lba;
    part->sector_count = count;
    part->sys_id       = sys_id;

    kstrncpy(part->block_dev.name, name, sizeof(part->block_dev.name));
    part->block_dev.total_sectors = count;
    part->block_dev.sector_size   = master->sector_size;
    part->block_dev.flags         = BLOCKDEV_FLAG_PARTITION | BLOCKDEV_FLAG_READONLY;
    part->block_dev.read_sectors  = partition_read_sectors;
    part->block_dev.write_sectors = partition_write_sectors;
    part->block_dev.priv_data     = part;

    blockdev_register(&part->block_dev);
}

static int partition_has_valid_gpt_header(block_device_t* master) {
    if (!master || !master->read_sectors ||
        master->sector_size != 512 || master->total_sectors < 2) {
        return 0;
    }
    uint8_t sector[512] = {0};
    if (master->read_sectors(master, 1, 1, sector) != 0) return 0;
    const gpt_header_t* gpt = (const gpt_header_t*)sector;
    return gpt->signature == 0x5452415020494645ULL;
}

int partition_parse(block_device_t* master) {
    if (!master || !master->read_sectors || master->total_sectors == 0) {
        return -1;
    }
    if (master->sector_size != 512) {
        klog(LOG_WARNING, "Partition discovery: unsupported sector size; disk unchanged");
        return -2;
    }

    uint8_t sector_buf[512] = {0};
    if (master->read_sectors(master, 0, 1, sector_buf) != 0) {
        klog(LOG_ERROR, "Partition discovery: failed to read LBA 0");
        return -1;
    }

    mbr_sector_t* mbr = (mbr_sector_t*)sector_buf;
    if (mbr->signature != 0xAA55) {
        if (partition_has_valid_gpt_header(master)) {
            klog(LOG_WARNING, "GPT detected; enumeration is not implemented; disk unchanged");
            return -2;
        }
        klog(LOG_WARNING, "No supported partition table; disk left unchanged");
        return -1;
    }

    /* A protective MBR indicates GPT; never expose it as a normal volume. */
    for (int i = 0; i < 4; ++i) {
        if (mbr->entries[i].sys_id == 0xEE) {
            if (partition_has_valid_gpt_header(master)) {
                klog(LOG_WARNING, "GPT detected; GPT enumeration is not implemented; disk unchanged");
            } else {
                klog(LOG_WARNING, "Protective GPT entry detected; disk unchanged");
            }
            return -2;
        }
    }

    klog(LOG_INFO, "Valid MBR detected; scanning existing primary partitions");
    int valid_entries = 0;
    for (int i = 0; i < 4; ++i) {
        const uint8_t sys_id = mbr->entries[i].sys_id;
        const uint64_t start = mbr->entries[i].start_lba;
        const uint64_t count = mbr->entries[i].total_sectors;
        if (sys_id == 0 || count == 0) continue;

        /* Validate bounds before publishing a partition device. */
        if (start == 0 || start >= master->total_sectors ||
            count > master->total_sectors - start) {
            klog(LOG_WARNING, "Partition discovery: skipped out-of-range MBR entry");
            continue;
        }

        char pname[32] = {0};
        kstrncpy(pname, master->name, 24);
        int len = 0;
        while (pname[len] && len < (int)sizeof(pname) - 3) ++len;
        pname[len++] = 'p';
        pname[len++] = (char)('1' + i);
        pname[len] = '\0';
        create_partition_subdev(master, pname, start, count, sys_id);
        ++valid_entries;
    }

    if (valid_entries == 0) {
        klog(LOG_INFO, "MBR has no valid primary partitions; disk left unchanged");
    }
    return 0;
}
void partition_init(void) {
    klog(LOG_INFO, "Initializing read-only partition discovery (MBR; GPT detection only)");
    uint32_t count = blockdev_count();
    for (uint32_t i = 0; i < count; i++) {
        block_device_t* dev = blockdev_get_by_index(i);
        if (dev && !(dev->flags & BLOCKDEV_FLAG_PARTITION)) {
            partition_parse(dev);
        }
    }
}
