#include "blockdev.h"
#include "../memory/heap.h"
#include "../log/logger.h"

#define MAX_BLOCK_DEVICES 32

static block_device_t* devices[MAX_BLOCK_DEVICES];
static uint32_t device_count = 0;

static int kstrcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) { s1++; s2++; }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

void blockdev_init(void) {
    for (int i = 0; i < MAX_BLOCK_DEVICES; i++) {
        devices[i] = (void*)0;
    }
    device_count = 0;
    klog(LOG_INFO, "Block Device Subsystem Initialized");
}

int blockdev_register(block_device_t* dev) {
    if (!dev || device_count >= MAX_BLOCK_DEVICES) {
        return -1;
    }
    devices[device_count++] = dev;
    klog(LOG_INFO, "Registered block device");
    return 0;
}

block_device_t* blockdev_get(const char* name) {
    if (!name) return (void*)0;
    for (uint32_t i = 0; i < device_count; i++) {
        if (devices[i] && kstrcmp(devices[i]->name, name) == 0) {
            return devices[i];
        }
    }
    return (void*)0;
}

block_device_t* blockdev_get_by_index(uint32_t index) {
    if (index >= device_count) return (void*)0;
    return devices[index];
}

uint32_t blockdev_count(void) {
    return device_count;
}
