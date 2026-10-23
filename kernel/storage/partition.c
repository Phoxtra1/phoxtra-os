#include "partition.h"
#include "../memory/heap.h"
#include "../log/logger.h"

#define MBR_PARTITION_COUNT 4u
#define MBR_BOOT_ACTIVE 0x80u
#define MBR_TYPE_EXTENDED_CHS 0x05u
#define MBR_TYPE_EXTENDED_LBA 0x0Fu
#define MBR_TYPE_EXTENDED_LINUX 0x85u

static int partition_read_sectors(block_device_t* dev, uint64_t lba, uint32_t count, uint8_t* buffer) {
    if (!dev || !dev->priv_data || !buffer || count == 0) return -1;
    partition_device_t* part = (partition_device_t*)dev->priv_data;
    if (!part->master_dev || !part->master_dev->read_sectors) return -1;
    if (lba >= part->sector_count || (uint64_t)count > part->sector_count - lba) return -1;
    if (part->start_lba >= part->master_dev->total_sectors ||
        part->sector_count > part->master_dev->total_sectors - part->start_lba) return -1;
    if (dev->sector_size != part->master_dev->sector_size) return -1;
    return part->master_dev->read_sectors(part->master_dev, part->start_lba + lba, count, buffer);
}

/* Writes to discovered partitions remain disabled until persistent storage has a
   separately reviewed write policy, filesystem, flush semantics, and recovery path. */
static int partition_write_sectors(block_device_t* dev, uint64_t lba, uint32_t count, const uint8_t* buffer) {
    (void)dev;
    (void)lba;
    (void)count;
    (void)buffer;
    return -1;
}

static void kstrncpy(char* dst, const char* src, int maxlen) {
    if (!dst || maxlen <= 0) return;
    int i = 0;
    if (src) {
        for (; i < maxlen - 1 && src[i]; ++i) dst[i] = src[i];
    }
    dst[i] = '\0';
}

static int is_extended_partition(uint8_t type) {
    return type == MBR_TYPE_EXTENDED_CHS ||
           type == MBR_TYPE_EXTENDED_LBA ||
           type == MBR_TYPE_EXTENDED_LINUX;
}

static int ranges_overlap(uint64_t a_start, uint64_t a_count,
                          uint64_t b_start, uint64_t b_count) {
    /* Callers validate non-zero counts and end bounds before using this helper. */
    return a_start < b_start + b_count && b_start < a_start + a_count;
}

static int create_partition_subdev(block_device_t* master, const char* name,
                                   uint64_t start_lba, uint64_t count, uint8_t sys_id) {
    partition_device_t* part = (partition_device_t*)kmalloc(sizeof(partition_device_t));
    if (!part) {
        klog(LOG_ERROR, "Partition parser: allocation failed; entry skipped");
        return -1;
    }

    for (size_t i = 0; i < sizeof(*part); ++i) ((uint8_t*)part)[i] = 0;
    part->master_dev = master;
    part->start_lba = start_lba;
    part->sector_count = count;
    part->sys_id = sys_id;

    kstrncpy(part->block_dev.name, name, (int)sizeof(part->block_dev.name));
    part->block_dev.total_sectors = count;
    part->block_dev.sector_size = master->sector_size;
    part->block_dev.flags = BLOCKDEV_FLAG_PARTITION | BLOCKDEV_FLAG_READONLY |
        (master->flags & (BLOCKDEV_FLAG_SIMULATED | BLOCKDEV_FLAG_REMOVABLE));
    part->block_dev.read_sectors = partition_read_sectors;
    part->block_dev.write_sectors = partition_write_sectors;
    part->block_dev.priv_data = part;

    if (blockdev_register(&part->block_dev) != 0) {
        klog(LOG_WARNING, "Partition parser: block-device registry full; entry skipped");
        kfree(part);
        return -1;
    }
    return 0;
}

int partition_parse(block_device_t* master) {
    if (!master || !master->read_sectors || master->sector_size != 512u ||
        master->total_sectors < 2u) {
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

    uint8_t candidate[MBR_PARTITION_COUNT] = {0, 0, 0, 0};
    for (uint32_t i = 0; i < MBR_PARTITION_COUNT; ++i) {
        const mbr_partition_entry_t* entry = &mbr->entries[i];
        if (entry->sys_id == 0 && entry->total_sectors == 0) continue;

        if (entry->sys_id == 0 || entry->total_sectors == 0 ||
            (entry->boot_indicator != 0 && entry->boot_indicator != MBR_BOOT_ACTIVE)) {
            klog(LOG_WARNING, "Partition parser: malformed MBR entry; entry skipped");
            continue;
        }
        if (is_extended_partition(entry->sys_id)) {
            klog(LOG_INFO, "Partition parser: extended partition skipped; EBR chains are not implemented");
            continue;
        }

        const uint64_t start = entry->start_lba;
        const uint64_t count = entry->total_sectors;
        if (start == 0 || start >= master->total_sectors ||
            count > master->total_sectors - start) {
            klog(LOG_WARNING, "Partition parser: invalid partition bounds; entry skipped");
            continue;
        }
        candidate[i] = 1;
    }

    /* Reject both sides of overlapping partition declarations, regardless of order. */
    for (uint32_t i = 0; i < MBR_PARTITION_COUNT; ++i) {
        if (!candidate[i]) continue;
        const mbr_partition_entry_t* a = &mbr->entries[i];
        for (uint32_t j = i + 1; j < MBR_PARTITION_COUNT; ++j) {
            if (!candidate[j]) continue;
            const mbr_partition_entry_t* b = &mbr->entries[j];
            if (ranges_overlap(a->start_lba, a->total_sectors,
                              b->start_lba, b->total_sectors)) {
                candidate[i] = 0;
                candidate[j] = 0;
                klog(LOG_WARNING, "Partition parser: overlapping MBR entries rejected");
            }
        }
    }

    uint32_t discovered = 0;
    for (uint32_t i = 0; i < MBR_PARTITION_COUNT; ++i) {
        if (!candidate[i]) continue;
        const mbr_partition_entry_t* entry = &mbr->entries[i];

        char pname[32];
        kstrncpy(pname, master->name, 24);
        int len = 0;
        while (len < (int)sizeof(pname) - 3 && pname[len]) ++len;
        pname[len++] = 'p';
        pname[len++] = (char)('1' + i);
        pname[len] = '\0';

        if (create_partition_subdev(master, pname, entry->start_lba,
                                    entry->total_sectors, entry->sys_id) == 0) {
            ++discovered;
        }
    }

    if (discovered == 0) {
        klog(LOG_INFO, "Partition parser: no usable partitions found; device left unchanged");
    } else {
        klog(LOG_INFO, "Partition parser: valid, non-overlapping MBR partitions exposed read-only");
    }
    return 0;
}

void partition_init(void) {
    klog(LOG_INFO, "Partition discovery is read-only; automatic partition creation/formatting disabled");
    uint32_t count = blockdev_count();
    for (uint32_t i = 0; i < count; ++i) {
        block_device_t* dev = blockdev_get_by_index(i);
        if (dev && !(dev->flags & BLOCKDEV_FLAG_PARTITION)) {
            (void)partition_parse(dev);
        }
    }
}
