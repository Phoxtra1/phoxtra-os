#include "../kernel/storage/partition.h"
#include "../kernel/storage/blockdev.h"
#include "../kernel/memory/heap.h"
#include "../kernel/log/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static mbr_sector_t test_mbr;
static uint64_t last_read_lba;
static unsigned read_calls;
static unsigned write_calls;
static int fail_allocations;
static unsigned frees;

void klog(log_level_t level, const char* message) { (void)level; (void)message; }
void* kmalloc(size_t size) { return fail_allocations ? NULL : malloc(size); }
void kfree(void* ptr) { if (ptr) { ++frees; free(ptr); } }

static int fake_read(block_device_t* dev, uint64_t lba, uint32_t count, uint8_t* buffer) {
    if (!dev || !buffer || count == 0 || lba >= dev->total_sectors ||
        (uint64_t)count > dev->total_sectors - lba || count > 4) return -1;
    ++read_calls;
    last_read_lba = lba;
    if (lba == 0 && count == 1) {
        memcpy(buffer, &test_mbr, sizeof(test_mbr));
    } else {
        memset(buffer, 0xA5, (size_t)count * dev->sector_size);
    }
    return 0;
}

static int fake_write(block_device_t* dev, uint64_t lba, uint32_t count, const uint8_t* buffer) {
    (void)dev; (void)lba; (void)count; (void)buffer;
    ++write_calls;
    return 0;
}

static void set_entry(unsigned index, uint8_t boot, uint8_t type, uint32_t start, uint32_t count) {
    mbr_partition_entry_t* e = &test_mbr.entries[index];
    memset(e, 0, sizeof(*e));
    e->boot_indicator = boot;
    e->sys_id = type;
    e->start_lba = start;
    e->total_sectors = count;
}

static void valid_mbr(void) {
    memset(&test_mbr, 0, sizeof(test_mbr));
    test_mbr.signature = 0xAA55u;
}

static void setup_master(block_device_t* master) {
    memset(master, 0, sizeof(*master));
    memcpy(master->name, "testdisk", sizeof("testdisk"));
    master->total_sectors = 100000;
    master->sector_size = 512;
    master->flags = BLOCKDEV_FLAG_SIMULATED | BLOCKDEV_FLAG_READONLY;
    master->read_sectors = fake_read;
    master->write_sectors = fake_write;
    master->priv_data = NULL;
    blockdev_init();
    (void)blockdev_register(master);
    read_calls = 0;
    write_calls = 0;
    last_read_lba = UINT64_MAX;
    fail_allocations = 0;
}

#define CHECK(label, expression) do { \
    if (!(expression)) { fprintf(stderr, "FAIL: %s\n", label); return 1; } \
    printf("PASS: %s\n", label); \
} while (0)

int main(void) {
    block_device_t master;
    block_device_t fillers[31];
    uint8_t sector[512];

    /* Malformed, overlapping, extended, and out-of-range entries must not appear. */
    setup_master(&master);
    valid_mbr();
    set_entry(0, 0, 0x83, 2048, 2048);
    set_entry(1, 0, 0x83, 3000, 2048); /* Overlaps entry 0. */
    set_entry(2, 0, 0x0F, 8192, 4096); /* Extended partition chains unsupported. */
    set_entry(3, 0x7F, 0x83, 16000, 1024); /* Invalid boot indicator. */
    CHECK("valid MBR is parsed without failing", partition_parse(&master) == 0);
    CHECK("overlapping entries are both rejected", blockdev_count() == 1);
    CHECK("partition discovery performs no writes", write_calls == 0);

    /* Valid, disjoint partition extents are exposed as simulated, read-only children. */
    setup_master(&master);
    valid_mbr();
    set_entry(0, 0, 0x83, 2048, 2048);
    set_entry(1, 0x80, 0x0C, 8192, 1024);
    CHECK("valid disjoint MBR partitions are exposed", partition_parse(&master) == 0 && blockdev_count() == 3);
    block_device_t* p1 = blockdev_get("testdiskp1");
    block_device_t* p2 = blockdev_get("testdiskp2");
    CHECK("partition names match their MBR table slots", p1 != NULL && p2 != NULL);
    CHECK("partition device remains read-only and simulated",
          p1 && (p1->flags & BLOCKDEV_FLAG_READONLY) &&
          (p1->flags & BLOCKDEV_FLAG_PARTITION) && (p1->flags & BLOCKDEV_FLAG_SIMULATED));
    CHECK("last sector of partition reads from expected parent LBA",
          p1 && p1->read_sectors(p1, 2047, 1, sector) == 0 &&
          last_read_lba == 4095 && sector[0] == 0xA5);
    unsigned before_bounds = read_calls;
    CHECK("read starting at partition end is rejected",
          p1 && p1->read_sectors(p1, 2048, 1, sector) == -1);
    CHECK("zero-sector reads are rejected", p1 && p1->read_sectors(p1, 0, 0, sector) == -1);
    CHECK("overflowing read LBA is rejected",
          p1 && p1->read_sectors(p1, UINT64_MAX, 1, sector) == -1);
    CHECK("invalid bounds do not reach the parent device", read_calls == before_bounds);
    CHECK("partition write callback remains disabled",
          p1 && p1->write_sectors && p1->write_sectors(p1, 0, 1, sector) == -1);
    CHECK("parsing same MBR cannot register duplicate partition names",
          partition_parse(&master) == 0 && blockdev_count() == 3);
    CHECK("storage parser has not written any sectors", write_calls == 0);

    /* Allocation failures and registry exhaustion must not publish phantom devices. */
    setup_master(&master);
    valid_mbr();
    set_entry(0, 0, 0x83, 2048, 2048);
    fail_allocations = 1;
    CHECK("partition allocation failure fails closed", partition_parse(&master) == 0 && blockdev_count() == 1);
    fail_allocations = 0;

    setup_master(&master);
    valid_mbr();
    set_entry(0, 0, 0x83, 2048, 2048);
    for (unsigned i = 0; i < 31; ++i) {
        memset(&fillers[i], 0, sizeof(fillers[i]));
        (void)snprintf(fillers[i].name, sizeof(fillers[i].name), "filler%u", i);
        fillers[i].total_sectors = 10;
        fillers[i].sector_size = 512;
        fillers[i].read_sectors = fake_read;
        CHECK("fill registry slot", blockdev_register(&fillers[i]) == 0);
    }
    CHECK("full registry has expected capacity", blockdev_count() == 32);
    unsigned frees_before = frees;
    CHECK("registry exhaustion does not publish phantom partition",
          partition_parse(&master) == 0 && blockdev_count() == 32 &&
          blockdev_get("testdiskp1") == NULL && frees > frees_before);
    CHECK("all partition discovery paths remain non-writing", write_calls == 0);

    /* Invalid sector-zero signature must reject discovery without side effects. */
    setup_master(&master);
    memset(&test_mbr, 0, sizeof(test_mbr));
    set_entry(0, 0, 0x83, 2048, 2048);
    CHECK("invalid MBR signature is rejected", partition_parse(&master) == -1 && blockdev_count() == 1);

    printf("Partition/storage regression tests passed.\n");
    return 0;
}
