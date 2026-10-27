#include "phoxdrive_hazard.h"
#include "phoxdrive.h"
#include "phoxdrive_audio.h"
#include "../../log/logger.h"

static hazard_record_t s_hazards[MAX_ACTIVE_HAZARDS];
static uint32_t s_next_id = 1;

static void kstrncpy(char* dest, const char* src, uint32_t n) {
    uint32_t i = 0;
    while (i < n - 1 && src[i] != '\0') {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

void phoxdrive_hazard_init(void) {
    for (uint32_t i = 0; i < MAX_ACTIVE_HAZARDS; i++) {
        s_hazards[i].active = false;
        s_hazards[i].hazard_id = 0;
    }
    klog(LOG_HARDWARE, "Phoxdrive Road Hazard & Obstacle Scanner online");
}

bool phoxdrive_register_hazard(hazard_type_t type, uint32_t distance_m, uint32_t severity, const char* desc) {
    int free_slot = -1;
    for (uint32_t i = 0; i < MAX_ACTIVE_HAZARDS; i++) {
        if (!s_hazards[i].active) {
            free_slot = (int)i;
            break;
        }
    }

    if (free_slot == -1) return false;

    s_hazards[free_slot].hazard_id = s_next_id++;
    s_hazards[free_slot].type = type;
    s_hazards[free_slot].distance_m = distance_m;
    s_hazards[free_slot].severity = severity;
    kstrncpy(s_hazards[free_slot].description, desc ? desc : "Road Obstacle", 64);
    s_hazards[free_slot].active = true;

    /* Automatically trigger audio speak-out alert depending on hazard type */
    switch (type) {
        case HAZARD_TYPE_BROKEN_VEHICLE:
            phoxdrive_speak_alert("Warning: Stationary broken vehicle detected ahead. Reduce speed and prepare to merge.");
            break;
        case HAZARD_TYPE_ACCIDENT_ZONE:
            phoxdrive_speak_alert("Caution: Approaching high accident-prone sector. Drive with extreme vigilance.");
            break;
        case HAZARD_TYPE_DEPLORABLE_ROAD:
            phoxdrive_speak_alert("Attention: Severe deplorable road condition and potholes detected ahead. Slow down.");
            break;
        case HAZARD_TYPE_OVERSPEED:
            phoxdrive_speak_alert("Overspeeding alert! Current vehicle speed exceeds the legal road limit.");
            break;
        default:
            phoxdrive_speak_alert(desc ? desc : "Caution: Hazard detected ahead.");
            break;
    }

    klog(LOG_HARDWARE, "Phoxdrive Hazard Registered and Voice Alert Triggered");
    return true;
}

void phoxdrive_clear_hazard(uint32_t hazard_id) {
    for (uint32_t i = 0; i < MAX_ACTIVE_HAZARDS; i++) {
        if (s_hazards[i].active && s_hazards[i].hazard_id == hazard_id) {
            s_hazards[i].active = false;
            klog(LOG_HARDWARE, "Phoxdrive Hazard Cleared");
            break;
        }
    }
}

void phoxdrive_clear_all_hazards(void) {
    for (uint32_t i = 0; i < MAX_ACTIVE_HAZARDS; i++) {
        s_hazards[i].active = false;
    }
    klog(LOG_HARDWARE, "Phoxdrive All Active Hazards Cleared");
}

uint32_t phoxdrive_get_active_hazard_count(void) {
    uint32_t count = 0;
    for (uint32_t i = 0; i < MAX_ACTIVE_HAZARDS; i++) {
        if (s_hazards[i].active) count++;
    }
    return count;
}

hazard_record_t* phoxdrive_get_hazard_at(uint32_t index) {
    uint32_t cur = 0;
    for (uint32_t i = 0; i < MAX_ACTIVE_HAZARDS; i++) {
        if (s_hazards[i].active) {
            if (cur == index) return &s_hazards[i];
            cur++;
        }
    }
    return 0;
}

const char* phoxdrive_hazard_type_to_str(hazard_type_t type) {
    switch (type) {
        case HAZARD_TYPE_BROKEN_VEHICLE:  return "Broken-down Vehicle Ahead";
        case HAZARD_TYPE_ACCIDENT_ZONE:   return "High Accident-Prone Zone";
        case HAZARD_TYPE_DEPLORABLE_ROAD: return "Deplorable Road / Potholes";
        case HAZARD_TYPE_OVERSPEED:       return "Vehicle Overspeeding Warning";
        case HAZARD_TYPE_OBSTACLE:        return "Roadway Obstacle";
        default:                          return "None";
    }
}
