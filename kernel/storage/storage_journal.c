#include "storage_journal.h"

#define JOURNAL_MAGIC       0x314A4850u /* "PHJ1" in little-endian bytes */
#define JOURNAL_VERSION     1u
#define JOURNAL_STATE_COMMIT 0xC01117EDu
#define JOURNAL_HEADER_SIZE 40u
#define JOURNAL_CRC_OFFSET  28u
#define JOURNAL_STATE_OFFSET 32u
#define JOURNAL_STATE_INV_OFFSET 36u

static uint32_t get_le32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint64_t get_le64(const uint8_t* p) {
    return (uint64_t)get_le32(p) | ((uint64_t)get_le32(p + 4) << 32);
}

static void put_le32(uint8_t* p, uint32_t value) {
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
    p[2] = (uint8_t)(value >> 16);
    p[3] = (uint8_t)(value >> 24);
}

static void put_le64(uint8_t* p, uint64_t value) {
    put_le32(p, (uint32_t)value);
    put_le32(p + 4, (uint32_t)(value >> 32));
}

static int all_zero(const uint8_t* bytes, uint32_t length) {
    if (!bytes) return 0;
    for (uint32_t i = 0; i < length; ++i) if (bytes[i] != 0) return 0;
    return 1;
}

/* CRC-32/ISO-HDLC. Used for torn/corrupt sector detection, not authentication. */
static uint32_t crc32(const uint8_t* bytes, uint32_t length) {
    uint32_t crc = 0xFFFFFFFFu;
    for (uint32_t i = 0; i < length; ++i) {
        crc ^= bytes[i];
        for (uint32_t bit = 0; bit < 8u; ++bit)
            crc = (crc >> 1) ^ ((crc & 1u) ? 0xEDB88320u : 0u);
    }
    return crc ^ 0xFFFFFFFFu;
}

/* 0 = empty, 1 = valid commit record, -1 = non-empty but invalid/corrupt. */
static int parse_record(const uint8_t sector[STORAGE_JOURNAL_SECTOR_SIZE],
                        uint64_t* sequence, uint64_t* target_lba,
                        uint32_t* data_crc) {
    if (!sector || !sequence || !target_lba || !data_crc) return -1;
    if (all_zero(sector, STORAGE_JOURNAL_SECTOR_SIZE)) return 0;

    if (get_le32(sector) != JOURNAL_MAGIC ||
        get_le32(sector + 4) != JOURNAL_VERSION ||
        get_le32(sector + JOURNAL_STATE_OFFSET) != JOURNAL_STATE_COMMIT ||
        get_le32(sector + JOURNAL_STATE_INV_OFFSET) != ~JOURNAL_STATE_COMMIT ||
        get_le64(sector + 8) == 0 ||
        !all_zero(sector + JOURNAL_HEADER_SIZE,
                  STORAGE_JOURNAL_SECTOR_SIZE - JOURNAL_HEADER_SIZE)) return -1;

    uint8_t check[STORAGE_JOURNAL_SECTOR_SIZE];
    for (uint32_t i = 0; i < STORAGE_JOURNAL_SECTOR_SIZE; ++i) check[i] = sector[i];
    uint32_t stored_crc = get_le32(check + JOURNAL_CRC_OFFSET);
    put_le32(check + JOURNAL_CRC_OFFSET, 0);
    if (stored_crc != crc32(check, STORAGE_JOURNAL_SECTOR_SIZE)) return -1;

    *sequence = get_le64(sector + 8);
    *target_lba = get_le64(sector + 16);
    *data_crc = get_le32(sector + 24);
    return 1;
}

static void make_record(uint8_t sector[STORAGE_JOURNAL_SECTOR_SIZE],
                        uint64_t sequence, uint64_t target_lba,
                        uint32_t data_crc) {
    for (uint32_t i = 0; i < STORAGE_JOURNAL_SECTOR_SIZE; ++i) sector[i] = 0;
    put_le32(sector, JOURNAL_MAGIC);
    put_le32(sector + 4, JOURNAL_VERSION);
    put_le64(sector + 8, sequence);
    put_le64(sector + 16, target_lba);
    put_le32(sector + 24, data_crc);
    put_le32(sector + JOURNAL_CRC_OFFSET, 0);
    put_le32(sector + JOURNAL_STATE_OFFSET, JOURNAL_STATE_COMMIT);
    put_le32(sector + JOURNAL_STATE_INV_OFFSET, ~JOURNAL_STATE_COMMIT);
    put_le32(sector + JOURNAL_CRC_OFFSET, crc32(sector, STORAGE_JOURNAL_SECTOR_SIZE));
}

static int target_is_allowed(const storage_journal_t* journal, uint64_t target_lba) {
    return journal && target_lba >= journal->target_start_lba &&
           target_lba - journal->target_start_lba < journal->target_sector_count;
}

static int journal_sector_read(storage_journal_t* journal, uint64_t lba,
                               uint8_t output[STORAGE_JOURNAL_SECTOR_SIZE]) {
    return journal->device->read_sectors(journal->device, lba, 1, output);
}

static int journal_sector_write(storage_journal_t* journal, uint64_t lba,
                                const uint8_t input[STORAGE_JOURNAL_SECTOR_SIZE]) {
    /* blockdev_write_sectors() flushes before it can return success. */
    return blockdev_write_sectors(journal->device, lba, 1, input);
}

static int clear_records(storage_journal_t* journal) {
    uint8_t zero[STORAGE_JOURNAL_SECTOR_SIZE] = {0};
    /* Clear B before A: after any interrupted clear, one durable commit remains. */
    if (journal_sector_write(journal, journal->journal_start_lba + 2u, zero) != 0)
        return -1;
    if (journal_sector_write(journal, journal->journal_start_lba, zero) != 0)
        return -1;
    return 0;
}

static int recover_locked(storage_journal_t* journal) {
    uint8_t record_a[STORAGE_JOURNAL_SECTOR_SIZE];
    uint8_t record_b[STORAGE_JOURNAL_SECTOR_SIZE];
    uint8_t data[STORAGE_JOURNAL_SECTOR_SIZE];
    uint64_t seq_a = 0, target_a = 0, seq_b = 0, target_b = 0;
    uint32_t crc_a = 0, crc_b = 0;

    journal->recovered = 0;
    if (journal_sector_read(journal, journal->journal_start_lba, record_a) != 0 ||
        journal_sector_read(journal, journal->journal_start_lba + 2u, record_b) != 0)
        return -1;

    int a = parse_record(record_a, &seq_a, &target_a, &crc_a);
    int b = parse_record(record_b, &seq_b, &target_b, &crc_b);
    if (a == 0 && b == 0) {
        journal->recovered = 1;
        return 0;
    }

    /* A torn/non-empty record with no independently valid peer is ambiguous.
       Fail closed instead of guessing whether a target write had occurred. */
    if ((a != 1 && b != 1) || (a < 0 && b == 0) || (b < 0 && a == 0))
        return -2;
    if (a == 1 && b == 1 &&
        (seq_a != seq_b || target_a != target_b || crc_a != crc_b))
        return -2;

    uint64_t sequence = (a == 1) ? seq_a : seq_b;
    uint64_t target = (a == 1) ? target_a : target_b;
    uint32_t expected_data_crc = (a == 1) ? crc_a : crc_b;
    if (!target_is_allowed(journal, target)) return -2;
    if (journal_sector_read(journal, journal->journal_start_lba + 1u, data) != 0)
        return -1;
    if (crc32(data, STORAGE_JOURNAL_SECTOR_SIZE) != expected_data_crc)
        return -2;

    /* Replay is idempotent: the same complete sector is safe to write again. */
    if (journal_sector_write(journal, target, data) != 0) return -1;
    if (clear_records(journal) != 0) return -1;

    journal->sequence = sequence;
    journal->recovered = 1;
    return 0;
}

int storage_journal_init(storage_journal_t* journal, block_device_t* device,
                         uint64_t journal_start_lba,
                         uint64_t target_start_lba,
                         uint64_t target_sector_count) {
    if (!journal) return -1;
    journal->initialized = 0;
    journal->recovered = 0;
    journal->busy = 0;
    journal->sequence = 0;

    if (!device || device->sector_size != STORAGE_JOURNAL_SECTOR_SIZE ||
        !device->read_sectors || !device->write_sectors ||
        !(device->flags & BLOCKDEV_FLAG_SIMULATED) ||
        !(device->flags & BLOCKDEV_FLAG_WRITE_ENABLED) ||
        (device->flags & BLOCKDEV_FLAG_READONLY)) return -1;
    if (device->total_sectors < STORAGE_JOURNAL_SECTOR_COUNT ||
        journal_start_lba > device->total_sectors - STORAGE_JOURNAL_SECTOR_COUNT ||
        target_sector_count == 0 || target_start_lba >= device->total_sectors ||
        target_sector_count > device->total_sectors - target_start_lba) return -2;

    uint64_t journal_end = journal_start_lba + STORAGE_JOURNAL_SECTOR_COUNT;
    uint64_t target_end = target_start_lba + target_sector_count;
    if (target_start_lba < journal_end && journal_start_lba < target_end) return -2;

    /* Also validates registry membership and that the durability hook succeeds. */
    if (blockdev_flush(device) != 0) return -1;

    journal->device = device;
    journal->journal_start_lba = journal_start_lba;
    journal->target_start_lba = target_start_lba;
    journal->target_sector_count = target_sector_count;
    journal->initialized = 1;
    return 0;
}

int storage_journal_recover(storage_journal_t* journal) {
    if (!journal || !journal->initialized) return -1;
    if (__sync_lock_test_and_set(&journal->busy, 1u)) return -1;
    int result = recover_locked(journal);
    __sync_lock_release(&journal->busy);
    return result;
}

int storage_journal_write_sector(storage_journal_t* journal, uint64_t target_lba,
                                 const uint8_t data[STORAGE_JOURNAL_SECTOR_SIZE]) {
    if (!journal || !journal->initialized || !data) return -1;
    if (__sync_lock_test_and_set(&journal->busy, 1u)) return -1;
    if (!journal->recovered || !target_is_allowed(journal, target_lba)) {
        __sync_lock_release(&journal->busy);
        return -1;
    }

    uint8_t record_a[STORAGE_JOURNAL_SECTOR_SIZE];
    uint8_t record_b[STORAGE_JOURNAL_SECTOR_SIZE];
    if (journal_sector_read(journal, journal->journal_start_lba, record_a) != 0 ||
        journal_sector_read(journal, journal->journal_start_lba + 2u, record_b) != 0 ||
        !all_zero(record_a, STORAGE_JOURNAL_SECTOR_SIZE) ||
        !all_zero(record_b, STORAGE_JOURNAL_SECTOR_SIZE)) {
        journal->recovered = 0;
        __sync_lock_release(&journal->busy);
        return -2;
    }
    if (journal->sequence == UINT64_MAX) {
        __sync_lock_release(&journal->busy);
        return -2;
    }

    uint64_t sequence = journal->sequence + 1u;
    uint32_t data_crc = crc32(data, STORAGE_JOURNAL_SECTOR_SIZE);
    uint8_t record[STORAGE_JOURNAL_SECTOR_SIZE];
    make_record(record, sequence, target_lba, data_crc);
    journal->recovered = 0;

    /* Write-ahead ordering: data, redundant commit records, target, then clear. */
    if (journal_sector_write(journal, journal->journal_start_lba + 1u, data) != 0 ||
        journal_sector_write(journal, journal->journal_start_lba, record) != 0 ||
        journal_sector_write(journal, journal->journal_start_lba + 2u, record) != 0 ||
        journal_sector_write(journal, target_lba, data) != 0 ||
        clear_records(journal) != 0) {
        __sync_lock_release(&journal->busy);
        return -2;
    }

    journal->sequence = sequence;
    journal->recovered = 1;
    __sync_lock_release(&journal->busy);
    return 0;
}
