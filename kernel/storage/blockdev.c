#include "blockdev.h"
#include "../memory/heap.h"
#include "../log/logger.h"

#define MAX_BLOCK_DEVICES 32

static block_device_t* devices[MAX_BLOCK_DEVICES];
static uint32_t device_count;

static int bounded_name_length(const char* name, size_t capacity, size_t* length_out) {
    if (!name || !length_out || capacity == 0) return -1;
    size_t length = 0;
    while (length < capacity && name[length]) ++length;
    if (length == 0 || length >= capacity) return -1;
    *length_out = length;
    return 0;
}

static int blockdev_is_registered(const block_device_t* dev) {
    if (!dev) return 0;
    for (uint32_t i = 0; i < device_count; ++i) {
        if (devices[i] == dev) return 1;
    }
    return 0;
}

static int blockdev_write_policy_valid(const block_device_t* dev) {
    return dev &&
           (dev->flags & BLOCKDEV_FLAG_SIMULATED) != 0 &&
           (dev->flags & BLOCKDEV_FLAG_WRITE_ENABLED) != 0 &&
           (dev->flags & BLOCKDEV_FLAG_READONLY) == 0 &&
           dev->write_sectors && dev->flush_sectors;
}

static int kstrcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) { ++s1; ++s2; }
    return (int)(unsigned char)*s1 - (int)(unsigned char)*s2;
}

void blockdev_init(void) {
    for (uint32_t i = 0; i < MAX_BLOCK_DEVICES; ++i) devices[i] = (void*)0;
    device_count = 0;
    klog(LOG_INFO, "Block Device Subsystem Initialized");
}

int blockdev_register(block_device_t* dev) {
    if (!dev || !dev->read_sectors || dev->sector_size == 0 || dev->total_sectors == 0 ||
        device_count >= MAX_BLOCK_DEVICES) {
        return -1;
    }
    if ((dev->flags & BLOCKDEV_FLAG_WRITE_ENABLED) &&
        (!(dev->flags & BLOCKDEV_FLAG_SIMULATED) ||
         (dev->flags & BLOCKDEV_FLAG_READONLY) ||
         !dev->write_sectors || !dev->flush_sectors)) {
        klog(LOG_WARNING, "Block-device registration rejected: unsafe write-enable policy");
        return -1;
    }
    size_t name_length = 0;
    if (bounded_name_length(dev->name, sizeof(dev->name), &name_length) != 0) {
        klog(LOG_WARNING, "Block-device registration rejected: invalid device name");
        return -1;
    }
    (void)name_length;

    for (uint32_t i = 0; i < device_count; ++i) {
        if (devices[i] == dev || (devices[i] && kstrcmp(devices[i]->name, dev->name) == 0)) {
            klog(LOG_WARNING, "Block-device registration rejected: duplicate pointer or name");
            return -1;
        }
    }

    devices[device_count++] = dev;
    klog(LOG_INFO, "Registered block device");
    return 0;
}

block_device_t* blockdev_get(const char* name) {
    if (!name) return (void*)0;
    for (uint32_t i = 0; i < device_count; ++i) {
        if (devices[i] && kstrcmp(devices[i]->name, name) == 0) return devices[i];
    }
    return (void*)0;
}

block_device_t* blockdev_get_by_index(uint32_t index) {
    if (index >= device_count) return (void*)0;
    return devices[index];
}

uint32_t blockdev_count(void) { return device_count; }

/* The only supported write path is opt-in, simulated storage with a durability
 * callback. Physical AHCI and discovered partition devices remain read-only. */
int blockdev_flush(block_device_t* dev) {
    if (!blockdev_is_registered(dev) || !blockdev_write_policy_valid(dev))
        return -1;
    return dev->flush_sectors(dev);
}

int blockdev_write_sectors(block_device_t* dev, uint64_t lba,
                           uint32_t count, const uint8_t* buffer) {
    if (!blockdev_is_registered(dev) || !blockdev_write_policy_valid(dev) ||
        !buffer || count == 0 || !dev->sector_size ||
        lba >= dev->total_sectors ||
        (uint64_t)count > dev->total_sectors - lba) {
        return -1;
    }

    if (dev->write_sectors(dev, lba, count, buffer) != 0)
        return -1;

    /* A successful call means the backend confirmed durability, not just that
     * data was copied into a volatile device/controller cache. */
    return blockdev_flush(dev) == 0 ? 0 : -2;
}
