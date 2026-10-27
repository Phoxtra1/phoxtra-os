#include "fieldsense.h"
#include "../hardware_manager.h"
#include "../../log/logger.h"

static fieldsense_telemetry_t s_fs_telemetry;
static phoxtra_device_t       s_fieldsense_device;

static void kstrncpy(char* dest, const char* src, uint32_t n) {
    uint32_t i = 0;
    while (i < n - 1 && src[i] != '\0') {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

void fieldsense_init(void) {
    s_fs_telemetry.soil_moisture_pct = 42;
    s_fs_telemetry.soil_temp_c = 24;
    s_fs_telemetry.air_humidity_pct = 68;
    s_fs_telemetry.soil_ph_e2 = 680; /* 6.8 */
    s_fs_telemetry.solar_battery_pct = 95;
    s_fs_telemetry.irrigation_valve_active = false;
    s_fs_telemetry.drought_stress_alert = false;

    /* Register device with Hardware Manager */
    s_fieldsense_device.device_id = 0x4653454E; /* 'FSEN' */
    kstrncpy(s_fieldsense_device.name, "FieldSense", sizeof(s_fieldsense_device.name));
    kstrncpy(s_fieldsense_device.serial, "PHOX-FLD-2027-A1", sizeof(s_fieldsense_device.serial));
    s_fieldsense_device.device_class = HARDWARE_CLASS_AGRICULTURE;
    s_fieldsense_device.firmware_rev = 0x00010000;
    s_fieldsense_device.telemetry_active = true;
    s_fieldsense_device.on_telemetry_event = 0;
    s_fieldsense_device.on_alert_triggered = 0;

    hardware_register_device(&s_fieldsense_device);
    klog(LOG_HARDWARE, "FieldSense Agricultural Hardware Subsystem registered in Phoxtra OS");
}

fieldsense_telemetry_t* fieldsense_get_telemetry(void) {
    return &s_fs_telemetry;
}

void fieldsense_set_moisture(uint32_t moisture_pct) {
    s_fs_telemetry.soil_moisture_pct = moisture_pct;
    if (moisture_pct < 20) {
        s_fs_telemetry.drought_stress_alert = true;
        klog(LOG_HARDWARE, "FieldSense: Drought stress alert! Soil moisture critical");
    } else {
        s_fs_telemetry.drought_stress_alert = false;
    }
}

void fieldsense_trigger_irrigation(bool activate) {
    s_fs_telemetry.irrigation_valve_active = activate;
    klog(LOG_HARDWARE, "FieldSense: Irrigation Solenoid Valve state updated");
}
