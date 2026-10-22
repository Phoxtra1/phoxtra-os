#include "phoxdrive.h"
#include "phoxdrive_hazard.h"
#include "phoxdrive_audio.h"
#include "phoxdrive_telemetry.h"
#include "../hardware_manager.h"
#include "../../log/logger.h"

static phoxdrive_state_t s_state;
static phoxtra_device_t  s_phoxdrive_device;

static void kstrncpy(char* dest, const char* src, uint32_t n) {
    uint32_t i = 0;
    while (i < n - 1 && src[i] != '\0') {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

static void on_phoxdrive_telemetry_event(const char* event_name, const void* payload) {
    (void)payload;
    (void)event_name;
    klog(LOG_HARDWARE, "Phoxdrive Telemetry Event Processed");
}

static void on_phoxdrive_alert_triggered(uint32_t alert_level, const char* message) {
    (void)alert_level;
    phoxdrive_speak_alert(message);
}

void phoxdrive_init(void) {
    s_state.speed_kmh = 45;
    s_state.speed_limit_kmh = 80;
    s_state.overspeed_detected = false;
    s_state.road_quality_score = 88;
    s_state.deplorable_road_ahead = false;
    s_state.broken_vehicle_ahead = false;
    s_state.broken_vehicle_distance_m = 0;
    s_state.accident_zone_ahead = false;
    s_state.active_alerts_count = 0;
    s_state.hud_mode_active = true;
    s_state.last_telemetry_tick = 0;
    kstrncpy(s_state.last_voice_alert, "Phoxdrive system active. Safety radar online.", sizeof(s_state.last_voice_alert));

    /* Initialize inner subsystems */
    phoxdrive_audio_init();
    phoxdrive_hazard_init();
    phoxdrive_telemetry_init();

    /* Register with Phoxtra Universal Hardware Device Manager */
    s_phoxdrive_device.device_id = 0x50445256; /* 'PDRV' */
    kstrncpy(s_phoxdrive_device.name, "Phoxdrive", sizeof(s_phoxdrive_device.name));
    kstrncpy(s_phoxdrive_device.serial, "PHOX-DRV-2026-X1", sizeof(s_phoxdrive_device.serial));
    s_phoxdrive_device.device_class = HARDWARE_CLASS_AUTOMOTIVE;
    s_phoxdrive_device.firmware_rev = 0x00010000; /* v1.0.0 */
    s_phoxdrive_device.telemetry_active = true;
    s_phoxdrive_device.on_telemetry_event = on_phoxdrive_telemetry_event;
    s_phoxdrive_device.on_alert_triggered = on_phoxdrive_alert_triggered;

    hardware_register_device(&s_phoxdrive_device);
    hardware_set_active_primary_class(HARDWARE_CLASS_AUTOMOTIVE);

    klog(LOG_HARDWARE, "Phoxtra Phoxdrive Subsystem fully initialized and running as native hardware service");
}

void phoxdrive_tick(void) {
    s_state.active_alerts_count = phoxdrive_get_active_hazard_count();
}

phoxdrive_state_t* phoxdrive_get_state(void) {
    s_state.active_alerts_count = phoxdrive_get_active_hazard_count();
    const char* last_spoken = phoxdrive_get_last_spoken_alert();
    if (last_spoken) {
        kstrncpy(s_state.last_voice_alert, last_spoken, sizeof(s_state.last_voice_alert));
    }
    return &s_state;
}

void phoxdrive_set_speed(uint32_t speed_kmh) {
    s_state.speed_kmh = speed_kmh;
    s_state.overspeed_detected = (speed_kmh > s_state.speed_limit_kmh);

    phoxdrive_telemetry_update(speed_kmh, 1000);

    if (s_state.overspeed_detected) {
        klog(LOG_HARDWARE, "Phoxdrive: Overspeed condition detected against legal limit");
    }
}

void phoxdrive_set_speed_limit(uint32_t limit_kmh) {
    s_state.speed_limit_kmh = limit_kmh;
    if (s_state.speed_kmh > limit_kmh) {
        s_state.overspeed_detected = true;
        phoxdrive_register_hazard(HAZARD_TYPE_OVERSPEED, 0, 3, "Exceeding Road Limit");
    } else {
        s_state.overspeed_detected = false;
    }
}

void phoxdrive_report_road_quality(uint32_t score_0_to_100) {
    if (score_0_to_100 > 100) score_0_to_100 = 100;
    s_state.road_quality_score = score_0_to_100;

    if (score_0_to_100 < 40) {
        s_state.deplorable_road_ahead = true;
        phoxdrive_register_hazard(HAZARD_TYPE_DEPLORABLE_ROAD, 150, 2, "Deplorable Road Surface Ahead");
    } else {
        s_state.deplorable_road_ahead = false;
    }
}

void phoxdrive_trigger_broken_vehicle(uint32_t distance_meters) {
    s_state.broken_vehicle_ahead = true;
    s_state.broken_vehicle_distance_m = distance_meters;
    phoxdrive_register_hazard(HAZARD_TYPE_BROKEN_VEHICLE, distance_meters, 3, "Broken-down Vehicle Ahead");
}

void phoxdrive_clear_broken_vehicle(void) {
    s_state.broken_vehicle_ahead = false;
    s_state.broken_vehicle_distance_m = 0;
    klog(LOG_HARDWARE, "Phoxdrive: Broken vehicle radar cleared");
}

void phoxdrive_trigger_accident_zone(bool approaching) {
    s_state.accident_zone_ahead = approaching;
    if (approaching) {
        phoxdrive_register_hazard(HAZARD_TYPE_ACCIDENT_ZONE, 300, 2, "Accident-Prone Zone Approaching");
    }
}

void phoxdrive_dispatch_voice_alert(const char* message) {
    phoxdrive_speak_alert(message);
    if (message) {
        kstrncpy(s_state.last_voice_alert, message, sizeof(s_state.last_voice_alert));
    }
}
