#include "../kernel/storage/storage_journal.h"
#include "../kernel/log/logger.h"
#include <stdio.h>
#include <string.h>

#define TEST_SECTORS 16u
#define TEST_BYTES (TEST_SECTORS * STORAGE_JOURNAL_SECTOR_SIZE)
#define JOURNAL_LBA 1u
#define TARGET_START 4u
#define TARGET_COUNT 8u
#define TARGET_LBA 4u
#define JOURNAL_MAGIC_TEST 0x314A4850u
#define JOURNAL_STATE_DONE_TEST 0xD06ECAFEu

typedef struct test_disk {
    uint8_t bytes[TEST_BYTES];
    unsigned read_calls;
    unsigned write_calls;
    unsigned flush_calls;
    unsigned fail_write_call;
    unsigned fail_flush_call;
    unsigned partial_write_bytes;
} test_disk_t;

static test_disk_t disk;
static block_device_t device;

void klog(log_level_t level, const char* message) { (void)level; (void)message; }

static int test_read(block_device_t* dev, uint64_t lba, uint32_t count, uint8_t* output) {
    if (!dev || !dev->priv_data || !output || count == 0 ||
        lba >= dev->total_sectors || (uint64_t)count > dev->total_sectors - lba)
        return -1;
    test_disk_t* d = (test_disk_t*)dev->priv_data;
    uint64_t offset = lba * dev->sector_size;
    uint64_t length = (uint64_t)count * dev->sector_size;
    if (offset > sizeof(d->bytes) || length > sizeof(d->bytes) - offset) return -1;
    ++d->read_calls;
    memcpy(output, d->bytes + offset, (size_t)length);
    return 0;
}

static int test_write(block_device_t* dev, uint64_t lba, uint32_t count,
                      const uint8_t* input) {
    if (!dev || !dev->priv_data || !input || count == 0 ||
        lba >= dev->total_sectors || (uint64_t)count > dev->total_sectors - lba)
        return -1;
    test_disk_t* d = (test_disk_t*)dev->priv_data;
    ++d->write_calls;
    uint64_t offset = lba * dev->sector_size;
    uint64_t length = (uint64_t)count * dev->sector_size;
    if (offset > sizeof(d->bytes) || length > sizeof(d->bytes) - offset) return -1;
    if (d->fail_write_call && d->write_calls == d->fail_write_call) {
        if (d->partial_write_bytes && d->partial_write_bytes < length)
            memcpy(d->bytes + offset, input, d->partial_write_bytes);
        return -1;
    }
    memcpy(d->bytes + offset, input, (size_t)length);
    return 0;
}

static int test_flush(block_device_t* dev) {
    if (!dev || !dev->priv_data) return -1;
    test_disk_t* d = (test_disk_t*)dev->priv_data;
    ++d->flush_calls;
    return (d->fail_flush_call && d->flush_calls == d->fail_flush_call) ? -1 : 0;
}

static void setup(unsigned readonly) {
    memset(&disk, 0, sizeof(disk));
    memset(&device, 0, sizeof(device));
    memcpy(device.name, "journal-test", sizeof("journal-test"));
    device.total_sectors = TEST_SECTORS;
    device.sector_size = STORAGE_JOURNAL_SECTOR_SIZE;
    device.flags = readonly
        ? (BLOCKDEV_FLAG_SIMULATED | BLOCKDEV_FLAG_READONLY)
        : (BLOCKDEV_FLAG_SIMULATED | BLOCKDEV_FLAG_WRITE_ENABLED);
    device.read_sectors = test_read;
    device.write_sectors = test_write;
    device.flush_sectors = test_flush;
    device.priv_data = &disk;
    blockdev_init();
    (void)blockdev_register(&device);
    for (uint64_t lba = TARGET_START; lba < TARGET_START + TARGET_COUNT; ++lba)
        memset(disk.bytes + lba * STORAGE_JOURNAL_SECTOR_SIZE, 0x11,
               STORAGE_JOURNAL_SECTOR_SIZE);
}

static int initialize(storage_journal_t* journal) {
    if (storage_journal_init(journal, &device, JOURNAL_LBA,
                             TARGET_START, TARGET_COUNT) != 0) return -1;
    return storage_journal_recover(journal);
}

static int sector_has(uint64_t lba, uint8_t value) {
    const uint8_t* p = disk.bytes + lba * STORAGE_JOURNAL_SECTOR_SIZE;
    for (uint32_t i = 0; i < STORAGE_JOURNAL_SECTOR_SIZE; ++i)
        if (p[i] != value) return 0;
    return 1;
}

static uint32_t get_le32_test(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static void put_le32_test(uint8_t* p, uint32_t x) {
    p[0]=(uint8_t)x; p[1]=(uint8_t)(x>>8); p[2]=(uint8_t)(x>>16); p[3]=(uint8_t)(x>>24);
}
static void put_le64_test(uint8_t* p, uint64_t x) {
    put_le32_test(p, (uint32_t)x); put_le32_test(p+4, (uint32_t)(x>>32));
}
static uint32_t crc32_test(const uint8_t* bytes, uint32_t length) {
    uint32_t crc = 0xFFFFFFFFu;
    for (uint32_t i=0; i<length; ++i) {
        crc ^= bytes[i];
        for (uint32_t b=0; b<8u; ++b)
            crc = (crc >> 1) ^ ((crc & 1u) ? 0xEDB88320u : 0u);
    }
    return crc ^ 0xFFFFFFFFu;
}
static void refill_record_crc(uint8_t* record_sector) {
    put_le32_test(record_sector + 28u, 0);
    put_le32_test(record_sector + 28u,
                  crc32_test(record_sector, STORAGE_JOURNAL_SECTOR_SIZE));
}
static int record_is_done(uint64_t lba) {
    const uint8_t* p = disk.bytes + lba * STORAGE_JOURNAL_SECTOR_SIZE;
    uint8_t copy[STORAGE_JOURNAL_SECTOR_SIZE];
    if (get_le32_test(p) != JOURNAL_MAGIC_TEST || get_le32_test(p + 4u) != 1u ||
        get_le32_test(p + 32u) != JOURNAL_STATE_DONE_TEST ||
        get_le32_test(p + 36u) != ~JOURNAL_STATE_DONE_TEST) return 0;
    memcpy(copy, p, sizeof(copy));
    uint32_t stored = get_le32_test(copy + 28u);
    put_le32_test(copy + 28u, 0);
    return stored == crc32_test(copy, sizeof(copy));
}

#define CHECK(label, expression) do { \
    if (!(expression)) { fprintf(stderr, "FAIL: %s\\n", label); return 1; } \
    printf("PASS: %s\\n", label); \
} while (0)

int main(void) {
    storage_journal_t journal;
    uint8_t replacement[STORAGE_JOURNAL_SECTOR_SIZE];

    /* Normal write path: never touches sectors outside its journal/target policy. */
    setup(0);
    memset(replacement, 0xA5, sizeof(replacement));
    CHECK("writable simulated test device registers", blockdev_get("journal-test") == &device);
    CHECK("journal initializes and clean recovery succeeds", initialize(&journal) == 0);
    unsigned writes_before = disk.write_calls;
    CHECK("write is refused before explicit recovery", 
          (journal.recovered = 0, storage_journal_write_sector(&journal, TARGET_LBA, replacement) != 0));
    CHECK("refused write does not touch media", disk.write_calls == writes_before);
    CHECK("journal can be recovered before use", storage_journal_recover(&journal) == 0);
    CHECK("single-sector transaction commits", storage_journal_write_sector(&journal, TARGET_LBA, replacement) == 0);
    CHECK("committed target contains new complete sector", sector_has(TARGET_LBA, 0xA5));
    CHECK("redundant durable DONE markers are retained",
          record_is_done(JOURNAL_LBA) && record_is_done(JOURNAL_LBA + 2u));
    CHECK("completed journal recovers idempotently", storage_journal_recover(&journal) == 0 &&
          sector_has(TARGET_LBA, 0xA5));

    /* Reusing the journal must preserve the previous committed generation until
       the new commit marker is durable, and replay only a committed new generation. */
    memset(replacement, 0xB6, sizeof(replacement));
    disk.fail_write_call = disk.write_calls + 2u; /* payload write succeeds; new A commit fails */
    CHECK("uncommitted replacement failure is reported",
          storage_journal_write_sector(&journal, TARGET_LBA + 1u, replacement) != 0);
    disk.fail_write_call = 0;
    CHECK("old generation survives failure before new commit",
          storage_journal_recover(&journal) == 0 &&
          sector_has(TARGET_LBA, 0xA5) && sector_has(TARGET_LBA + 1u, 0x11));
    memset(replacement, 0xC7, sizeof(replacement));
    disk.fail_write_call = disk.write_calls + 3u; /* new A commit persists; B commit fails */
    CHECK("partially committed replacement reports failure",
          storage_journal_write_sector(&journal, TARGET_LBA + 1u, replacement) != 0);
    disk.fail_write_call = 0;
    CHECK("new committed generation replays over old DONE generation",
          storage_journal_recover(&journal) == 0 &&
          sector_has(TARGET_LBA, 0xA5) && sector_has(TARGET_LBA + 1u, 0xC7));
    CHECK("both redundant records finish as DONE",
          record_is_done(JOURNAL_LBA) && record_is_done(JOURNAL_LBA + 2u));
    memset(replacement, 0xA5, sizeof(replacement));

    /* Reject out-of-range targets and any journal/target overlap at setup. */
    CHECK("target outside allowed range is refused",
          storage_journal_write_sector(&journal, TARGET_START + TARGET_COUNT, replacement) != 0);
    CHECK("journal metadata sector cannot be a target",
          storage_journal_write_sector(&journal, JOURNAL_LBA, replacement) != 0);
    storage_journal_t bad_layout;
    CHECK("overlapping journal and target ranges are refused",
          storage_journal_init(&bad_layout, &device, JOURNAL_LBA, JOURNAL_LBA + 1u, 4u) != 0);

    /* Read-only hardware policy is a hard stop, even with callbacks present. */
    setup(1);
    storage_journal_t readonly_journal;
    CHECK("read-only device refuses journal initialization",
          storage_journal_init(&readonly_journal, &device, JOURNAL_LBA, TARGET_START, TARGET_COUNT) != 0);
    CHECK("guarded block write refuses read-only device",
          blockdev_write_sectors(&device, TARGET_LBA, 1, replacement) != 0 && disk.write_calls == 0);

    /* Inject a failure at each of the six write stages and then reboot/recover.
       Failures before a valid commit leave the old target; later failures replay new data. */
    for (unsigned fail_at = 1; fail_at <= 6u; ++fail_at) {
        setup(0);
        storage_journal_t crash_journal;
        CHECK("crash-cut test setup succeeds", initialize(&crash_journal) == 0);
        disk.fail_write_call = fail_at;
        CHECK("interrupted transaction reports failure",
              storage_journal_write_sector(&crash_journal, TARGET_LBA, replacement) != 0);
        disk.fail_write_call = 0;
        disk.partial_write_bytes = 0;
        CHECK("post-crash recovery succeeds for a non-torn write",
              storage_journal_recover(&crash_journal) == 0);
        if (fail_at <= 2u) {
            CHECK("pre-commit interruption preserves old target", sector_has(TARGET_LBA, 0x11));
        } else {
            CHECK("committed interruption replays new target", sector_has(TARGET_LBA, 0xA5));
        }
        CHECK("recovery leaves metadata clean",
              record_is_done(JOURNAL_LBA) && record_is_done(JOURNAL_LBA + 2u));
    }

    /* A genuinely torn metadata sector is ambiguous and must not be guessed at. */
    setup(0);
    storage_journal_t torn_journal;
    CHECK("torn-write test setup succeeds", initialize(&torn_journal) == 0);
    disk.fail_write_call = 2u; /* journal data succeeds, first metadata copy tears */
    disk.partial_write_bytes = 28u; /* header present, checksum absent */
    CHECK("torn metadata transaction reports write failure",
          storage_journal_write_sector(&torn_journal, TARGET_LBA, replacement) != 0);
    disk.fail_write_call = 0;
    CHECK("corrupt lone metadata record fails closed", storage_journal_recover(&torn_journal) != 0);
    CHECK("failed recovery never changes target sector", sector_has(TARGET_LBA, 0x11));

    /* Torn DONE writes are recoverable from the independent commit/complete peer. */
    setup(0);
    storage_journal_t torn_done_a;
    CHECK("torn DONE-A test setup succeeds", initialize(&torn_done_a) == 0);
    disk.fail_write_call = 5u;
    disk.partial_write_bytes = 28u;
    CHECK("torn DONE-A transaction returns failure",
          storage_journal_write_sector(&torn_done_a, TARGET_LBA, replacement) != 0);
    disk.fail_write_call = 0;
    disk.partial_write_bytes = 0;
    CHECK("valid peer repairs torn DONE-A", storage_journal_recover(&torn_done_a) == 0);
    CHECK("torn DONE-A recovery retains target and matching DONE records",
          sector_has(TARGET_LBA, 0xA5) &&
          record_is_done(JOURNAL_LBA) && record_is_done(JOURNAL_LBA + 2u));

    setup(0);
    storage_journal_t torn_done_b;
    CHECK("torn DONE-B test setup succeeds", initialize(&torn_done_b) == 0);
    disk.fail_write_call = 6u;
    disk.partial_write_bytes = 28u;
    CHECK("torn DONE-B transaction returns failure",
          storage_journal_write_sector(&torn_done_b, TARGET_LBA, replacement) != 0);
    disk.fail_write_call = 0;
    disk.partial_write_bytes = 0;
    CHECK("valid peer repairs torn DONE-B", storage_journal_recover(&torn_done_b) == 0);
    CHECK("torn DONE-B recovery retains target and matching DONE records",
          sector_has(TARGET_LBA, 0xA5) &&
          record_is_done(JOURNAL_LBA) && record_is_done(JOURNAL_LBA + 2u));

    /* Corrupt journal payload after both commit records exist: never replay unchecked data. */
    setup(0);
    storage_journal_t corrupt_data_journal;
    CHECK("corrupt-payload test setup succeeds", initialize(&corrupt_data_journal) == 0);
    disk.fail_write_call = 4u; /* fail target write after both records became durable */
    CHECK("pending transaction is left after target-write failure",
          storage_journal_write_sector(&corrupt_data_journal, TARGET_LBA, replacement) != 0);
    disk.fail_write_call = 0;
    disk.bytes[(JOURNAL_LBA + 1u) * STORAGE_JOURNAL_SECTOR_SIZE + 17u] ^= 0x80u;
    CHECK("payload checksum corruption is rejected", storage_journal_recover(&corrupt_data_journal) != 0);
    CHECK("corrupt payload is never written to target", sector_has(TARGET_LBA, 0x11));

    /* Two individually valid but conflicting records are ambiguous and fail closed. */
    setup(0);
    storage_journal_t conflict_journal;
    CHECK("conflicting-record test setup succeeds", initialize(&conflict_journal) == 0);
    disk.fail_write_call = 4u;
    CHECK("leave a committed journal for conflict test",
          storage_journal_write_sector(&conflict_journal, TARGET_LBA, replacement) != 0);
    disk.fail_write_call = 0;
    uint8_t* record_b = disk.bytes + (JOURNAL_LBA + 2u) * STORAGE_JOURNAL_SECTOR_SIZE;
    put_le64_test(record_b + 16u, TARGET_LBA + 1u);
    refill_record_crc(record_b);
    CHECK("valid conflicting metadata copies fail closed",
          storage_journal_recover(&conflict_journal) != 0);
    CHECK("conflicting records do not alter target", sector_has(TARGET_LBA, 0x11));

    /* A reported flush error after writing the first commit record remains recoverable. */
    setup(0);
    storage_journal_t flush_journal;
    CHECK("flush-failure test setup succeeds", initialize(&flush_journal) == 0);
    disk.fail_flush_call = disk.flush_calls + 2u; /* data flush succeeds; metadata A flush fails */
    CHECK("flush failure is reported to caller",
          storage_journal_write_sector(&flush_journal, TARGET_LBA, replacement) != 0);
    disk.fail_flush_call = 0;
    CHECK("flush failure recovers through durable journal metadata",
          storage_journal_recover(&flush_journal) == 0 && sector_has(TARGET_LBA, 0xA5));

    /* Physical-style devices are never allowed to use the guarded write API. */
    setup(0);
    device.flags = BLOCKDEV_FLAG_READONLY; /* Remove simulated/write-enabled capability. */
    CHECK("non-simulated physical-style write is refused",
          blockdev_write_sectors(&device, TARGET_LBA, 1, replacement) != 0 && disk.write_calls == 0);

    printf("Storage journal failure-injection tests passed.\\n");
    return 0;
}
