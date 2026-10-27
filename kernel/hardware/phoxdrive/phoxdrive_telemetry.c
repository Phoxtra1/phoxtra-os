#include "phoxdrive_telemetry.h"
#include "phoxdrive.h"
#include "phoxdrive_hazard.h"
#include "phoxdrive_audio.h"
#include "../../log/logger.h"

static vehicle_telemetry_t s_telemetry;

void phoxdrive_telemetry_init(void) {
    s_telemetry.speed_kmh = 45;
    s_telemetry.speed_limit_kmh = 80;
    s_telemetry.accel_x_mg = 0;
    s_telemetry.accel_y_mg = 0;
    s_telemetry.accel_z_mg = 1000; /* 1G normal gravity */
    s_telemetry.latitude_e6 = 5603700;   /* Example default coordinates (Accra, GH: 5.6037 N) */
    s_telemetry.longitude_e6 = -187000;  /* -0.1870 W */
    s_telemetry.heading_deg = 90;
    s_telemetry.road_vibration_index = 12;
    s_telemetry.pothole_impact_detected = false;

    klog(LOG_HARDWARE, "Phoxdrive Vehicle Telemetry & IMU Ingestion initialized");
}

void phoxdrive_telemetry_update(uint32_t speed_kmh, int32_t z_bump_mg) {
    s_telemetry.speed_kmh = speed_kmh;
    s_telemetry.accel_z_mg = z_bump_mg;

    /* Overspeeding logic */
    if (speed_kmh > s_telemetry.speed_limit_kmh + 5) {
        phoxdrive_register_hazard(HAZARD_TYPE_OVERSPEED, 0, 3, "Speed Limit Exceeded");
    }

    /* Pothole / deplorable road vertical impact detection */
    /* Normal gravity is ~1000 mg. Severe vertical shock > 1800 mg indicates pothole impact */
    if (z_bump_mg > 1800 || z_bump_mg < 200) {
        s_telemetry.pothole_impact_detected = true;
        s_telemetry.road_vibration_index = 85;
        phoxdrive_register_hazard(HAZARD_TYPE_DEPLORABLE_ROAD, 20, 2, "Severe Pothole Impact Encountered");
    } else {
        s_telemetry.pothole_impact_detected = false;
        s_telemetry.road_vibration_index = 15;
    }
}

vehicle_telemetry_t* phoxdrive_get_telemetry(void) {
    return &s_telemetry;
}

void phoxdrive_set_coordinates(int32_t lat_e6, int32_t lon_e6, uint32_t heading) {
    s_telemetry.latitude_e6 = lat_e6;
    s_telemetry.longitude_e6 = lon_e6;
    s_telemetry.heading_deg = heading % 360;
}
