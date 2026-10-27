#ifndef PHOXDRIVE_TELEMETRY_H
#define PHOXDRIVE_TELEMETRY_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct vehicle_telemetry {
    uint32_t speed_kmh;
    uint32_t speed_limit_kmh;
    int32_t  accel_x_mg;          /* Lateral acceleration (milli-g) */
    int32_t  accel_y_mg;          /* Longitudinal acceleration / braking (milli-g) */
    int32_t  accel_z_mg;          /* Vertical acceleration / pothole bumps (milli-g) */
    int32_t  latitude_e6;         /* Latitude * 1,000,000 */
    int32_t  longitude_e6;        /* Longitude * 1,000,000 */
    uint32_t heading_deg;
    uint32_t road_vibration_index;/* 0 to 100 vibration severity */
    bool     pothole_impact_detected;
} vehicle_telemetry_t;

void                 phoxdrive_telemetry_init(void);
void                 phoxdrive_telemetry_update(uint32_t speed_kmh, int32_t z_bump_mg);
vehicle_telemetry_t* phoxdrive_get_telemetry(void);
void                 phoxdrive_set_coordinates(int32_t lat_e6, int32_t lon_e6, uint32_t heading);

#ifdef __cplusplus
}
#endif

#endif /* PHOXDRIVE_TELEMETRY_H */
