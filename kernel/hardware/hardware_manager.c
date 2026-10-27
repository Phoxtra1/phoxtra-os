#include "hardware_manager.h"
#include "../log/logger.h"

static phoxtra_device_t s_device_registry[MAX_HARDWARE_DEVICES];
static uint32_t s_device_count = 0;
static hardware_class_t s_active_primary_class = HARDWARE_CLASS_AUTOMOTIVE;

static int kstrcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) { s1++; s2++; }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

static void __attribute__((unused)) kstrncpy(char* dest, const char* src, uint32_t n) {
    uint32_t i = 0;
    while (i < n - 1 && src[i] != '\0') {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

void hardware_manager_init(void) {
    s_device_count = 0;
    for (uint32_t i = 0; i < MAX_HARDWARE_DEVICES; i++) {
        s_device_registry[i].device_id = 0;
        s_device_registry[i].status = HARDWARE_STATUS_OFFLINE;
        s_device_registry[i].name[0] = '\0';
    }
    klog(LOG_HARDWARE, "Phoxtra Universal Hardware Device Manager initialized");
}

bool hardware_register_device(phoxtra_device_t* dev) {
    if (!dev || s_device_count >= MAX_HARDWARE_DEVICES) {
        return false;
    }

    /* Assign slot */
    uint32_t idx = s_device_count;
    s_device_registry[idx] = *dev;
    s_device_registry[idx].status = HARDWARE_STATUS_ONLINE;
    s_device_count++;

    klog(LOG_HARDWARE, "Registered Phoxtra hardware device in registry");
    return true;
}

phoxtra_device_t* hardware_get_device(uint32_t device_id) {
    for (uint32_t i = 0; i < s_device_count; i++) {
        if (s_device_registry[i].device_id == device_id) {
            return &s_device_registry[i];
        }
    }
    return 0;
}

phoxtra_device_t* hardware_get_device_by_name(const char* name) {
    if (!name) return 0;
    for (uint32_t i = 0; i < s_device_count; i++) {
        if (kstrcmp(s_device_registry[i].name, name) == 0) {
            return &s_device_registry[i];
        }
    }
    return 0;
}

uint32_t hardware_device_count(void) {
    return s_device_count;
}

phoxtra_device_t* hardware_get_device_at(uint32_t index) {
    if (index >= s_device_count) return 0;
    return &s_device_registry[index];
}

const char* hardware_class_to_string(hardware_class_t dev_class) {
    switch (dev_class) {
        case HARDWARE_CLASS_AUTOMOTIVE:     return "Automotive Telematics (Phoxdrive)";
        case HARDWARE_CLASS_AGRICULTURE:    return "AgTech & Soil Sensing (FieldSense)";
        case HARDWARE_CLASS_SMART_MOBILITY: return "Smart Mobility & Robotics";
        case HARDWARE_CLASS_INDUSTRIAL_IOT: return "Industrial IoT & Grid";
        case HARDWARE_CLASS_EDGE_GATEWAY:   return "Edge Telemetry Gateway";
        case HARDWARE_CLASS_MOBILE:         return "Mobile Handheld (Phoxtra-Touch)";
        case HARDWARE_CLASS_DESKTOP_LAPTOP: return "Desktop / Laptop Workstation";
        default:                            return "Generic Hardware Profile";
    }
}

const char* hardware_status_to_string(hardware_status_t status) {
    switch (status) {
        case HARDWARE_STATUS_ONLINE:       return "ONLINE";
        case HARDWARE_STATUS_ALERT:        return "HAZARD_ALERT";
        case HARDWARE_STATUS_DEGRADED:     return "DEGRADED";
        case HARDWARE_STATUS_INITIALIZING: return "INITIALIZING";
        case HARDWARE_STATUS_OFFLINE:
        default:                           return "OFFLINE";
    }
}

hardware_class_t hardware_get_active_primary_class(void) {
    return s_active_primary_class;
}

void hardware_set_active_primary_class(hardware_class_t dev_class) {
    s_active_primary_class = dev_class;
}
