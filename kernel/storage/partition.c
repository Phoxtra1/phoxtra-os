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
    if (!dev || !dev->priv_data) return -1;
    partition_device_t* part = (partition_device_t*)dev->priv_data;
    if (lba + count > part->sector_count) return -1;

    return part->master_dev->write_sectors(part->master_dev, part->start_lba + lba, count, buffer);
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
    part->block_dev.flags         = BLOCKDEV_FLAG_PARTITION;
    part->block_dev.read_sectors  = partition_read_sectors;
    part->block_dev.write_sectors = partition_write_sectors;
    part->block_dev.priv_data     = part;

    blockdev_register(&part->block_dev);
}

int partition_parse(block_device_t* master) {
    if (!master || !master->read_sectors) return -1;

    uint8_t sector_buf[512];
    if (master->read_sectors(master, 0, 1, sector_buf) != 0) {
        klog(LOG_ERROR, "Partition Parser: Failed to read LBA 0");
        return -1;
    }

    mbr_sector_t* mbr = (mbr_sector_t*)sector_buf;

    // Check MBR signature 0xAA55
    if (mbr->signature == 0xAA55) {
        klog(LOG_INFO, "Partition Parser: Valid MBR Header Detected");

        int valid_entries = 0;
        for (int i = 0; i < 4; i++) {
            if (mbr->entries[i].sys_id != 0 && mbr->entries[i].total_sectors > 0) {
                char pname[32];
                pname[0] = '\0';
                // Build partition name, e.g. sata0p1
                kstrncpy(pname, master->name, 24);
                int len = 0; while (pname[len]) len++;
                pname[len++] = 'p';
                pname[len++] = '1' + i;
                pname[len]   = '\0';

                create_partition_subdev(master, pname, mbr->entries[i].start_lba, mbr->entries[i].total_sectors, mbr->entries[i].sys_id);
                valid_entries++;
            }
        }

        if (valid_entries == 0) {
            klog(LOG_INFO, "Partition Parser: Initializing default GPT/MBR partition map");
            // Set up 3 standard partitions on master drive:
            // p1: /boot (Ext2, 8MB = 16384 sectors)
            // p2: /usr  (Ext2, 16MB = 32768 sectors)
            // p3: /home (FAT32, 8MB = 16384 sectors)
            char p1_name[32], p2_name[32], p3_name[32];
            kstrncpy(p1_name, master->name, 24); kstrncpy(p2_name, master->name, 24); kstrncpy(p3_name, master->name, 24);
            int l = 0; while (p1_name[l]) l++;
            p1_name[l] = 'p'; p1_name[l+1] = '1'; p1_name[l+2] = '\0';
            p2_name[l] = 'p'; p2_name[l+1] = '2'; p2_name[l+2] = '\0';
            p3_name[l] = 'p'; p3_name[l+1] = '3'; p3_name[l+2] = '\0';

            create_partition_subdev(master, p1_name, 2048, 16384, 0x83);  // Ext2 Linux
            create_partition_subdev(master, p2_name, 18432, 32768, 0x83); // Ext2 Linux
            create_partition_subdev(master, p3_name, 51200, 14336, 0x0C); // FAT32 LBA

            // Write MBR back to sector 0
            mbr->signature = 0xAA55;
            mbr->entries[0].start_lba = 2048;  mbr->entries[0].total_sectors = 16384; mbr->entries[0].sys_id = 0x83;
            mbr->entries[1].start_lba = 18432; mbr->entries[1].total_sectors = 32768; mbr->entries[1].sys_id = 0x83;
            mbr->entries[2].start_lba = 51200; mbr->entries[2].total_sectors = 14336; mbr->entries[2].sys_id = 0x0C;
            master->write_sectors(master, 0, 1, sector_buf);
        }
        return 0;
    }

    klog(LOG_WARNING, "Partition Parser: MBR signature missing, initializing MBR");
    mbr->signature = 0xAA55;
    mbr->entries[0].start_lba = 2048;  mbr->entries[0].total_sectors = 16384; mbr->entries[0].sys_id = 0x83;
    mbr->entries[1].start_lba = 18432; mbr->entries[1].total_sectors = 32768; mbr->entries[1].sys_id = 0x83;
    mbr->entries[2].start_lba = 51200; mbr->entries[2].total_sectors = 14336; mbr->entries[2].sys_id = 0x0C;
    master->write_sectors(master, 0, 1, sector_buf);

    create_partition_subdev(master, "sata0p1", 2048, 16384, 0x83);
    create_partition_subdev(master, "sata0p2", 18432, 32768, 0x83);
    create_partition_subdev(master, "sata0p3", 51200, 14336, 0x0C);

    return 0;
}

void partition_init(void) {
    klog(LOG_INFO, "Initializing MBR & GPT Partition Table Subsystem...");
    uint32_t count = blockdev_count();
    for (uint32_t i = 0; i < count; i++) {
        block_device_t* dev = blockdev_get_by_index(i);
        if (dev && !(dev->flags & BLOCKDEV_FLAG_PARTITION)) {
            partition_parse(dev);
        }
    }
}
