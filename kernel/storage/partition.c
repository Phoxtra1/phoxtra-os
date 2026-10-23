#include "partition.h"
#include "../memory/heap.h"
#include "../log/logger.h"

static int partition_read_sectors(block_device_t* dev, uint64_t lba, uint32_t count, uint8_t* buffer) {
    if (!dev || !dev->priv_data || !buffer) return -1;
    partition_device_t* part = (partition_device_t*)dev->priv_data;
    if (!part->master_dev || !part->master_dev->read_sectors) return -1;
    if (lba > part->sector_count || (uint64_t)count > part->sector_count - lba) return -1;
    if (part->start_lba > part->master_dev->total_sectors ||
        part->sector_count > part->master_dev->total_sectors - part->start_lba) return -1;
    return part->master_dev->read_sectors(part->master_dev, part->start_lba + lba, count, buffer);
}

/* Discovered partitions default to read-only. Writes require a separate, explicit
   storage policy and are intentionally unavailable in this prototype. */
static int partition_write_sectors(block_device_t* dev, uint64_t lba, uint32_t count, const uint8_t* buffer) {
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

int partition_parse(block_device_t* master) {
    if (!master || !master->read_sectors || master->sector_size != 512u ||
        master->total_sectors == 0) {
        klog(LOG_WARNING, "Partition parser: unsupported device geometry; no changes made");
        return -1;
    }

    uint8_t sector_buf[512];
    if (master->read_sectors(master, 0, 1, sector_buf) != 0) {
        klog(LOG_WARNING, "Partition parser: failed to read sector zero; device left unchanged");
        return -1;
    }

    const mbr_sector_t* mbr = (const mbr_sector_t*)sector_buf;
    if (mbr->signature != 0xAA55u) {
        klog(LOG_INFO, "Partition parser: no valid MBR signature; device left unchanged");
        return -1;
    }

    uint32_t discovered = 0;
    for (uint32_t i = 0; i < 4u; ++i) {
        const mbr_partition_entry_t* entry = &mbr->entries[i];
        if (entry->sys_id == 0 || entry->total_sectors == 0) continue;

        const uint64_t start_lba = entry->start_lba;
        const uint64_t sector_count = entry->total_sectors;
        if (start_lba >= master->total_sectors ||
            sector_count > master->total_sectors - start_lba) {
            klog(LOG_WARNING, "Partition parser: invalid partition bounds; entry skipped");
            continue;
        }

        char pname[32];
        kstrncpy(pname, master->name, 24);
        int len = 0;
        while (pname[len] && len < 28) ++len;
        if (len > 28) {
            klog(LOG_WARNING, "Partition parser: device name too long; entry skipped");
            continue;
        }
        pname[len++] = 'p';
        pname[len++] = (char)('1' + i);
        pname[len] = '\0';

        create_partition_subdev(master, pname, start_lba, sector_count, entry->sys_id);
        ++discovered;
    }

    if (discovered == 0) {
        klog(LOG_INFO, "Partition parser: no usable partitions found; device left unchanged");
    } else {
        klog(LOG_INFO, "Partition parser: existing partition entries discovered read-only");
    }
    return 0;
}

void partition_init(void) {
    klog(LOG_INFO, "Partition discovery is read-only; automatic partition creation/formatting disabled");
    uint32_t count = blockdev_count();
    for (uint32_t i = 0; i < count; i++) {
        block_device_t* dev = blockdev_get_by_index(i);
        if (dev && !(dev->flags & BLOCKDEV_FLAG_PARTITION)) {
            partition_parse(dev);
        }
    }
}
