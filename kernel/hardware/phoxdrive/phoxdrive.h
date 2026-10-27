#ifndef PHOXDRIVE_H
#define PHOXDRIVE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ──────────────────────────────────────────────────────────
   Phoxtra OS - Phoxdrive Automotive Intelligence Subsystem
   Core controller running directly inside Phoxtra OS to empower
   the Phoxdrive in-vehicle hardware device.
   ────────────────────────────────────────────────────────── */

typedef struct phoxdrive_state {
    uint32_t speed_kmh;
    uint32_t speed_limit_kmh;
    bool     overspeed_detected;
    uint32_t road_quality_score;   /* 0 = Deplorable/Pot-holed, 100 = Pristine Highway */
    bool     deplorable_road_ahead;
    bool     broken_vehicle_ahead;
    uint32_t broken_vehicle_distance_m;
    bool     accident_zone_ahead;
    uint32_t active_alerts_count;
    char     last_voice_alert[128];
    uint64_t last_telemetry_tick;
    bool     hud_mode_active;
} phoxdrive_state_t;

/* Lifecycle */
void                phoxdrive_init(void);
void                phoxdrive_tick(void);
phoxdrive_state_t*  phoxdrive_get_state(void);

/* Speed & Telemetry Control */
void                phoxdrive_set_speed(uint32_t speed_kmh);
void                phoxdrive_set_speed_limit(uint32_t limit_kmh);

/* Road Conditions & Hazard Simulation / Ingestion */
void                phoxdrive_report_road_quality(uint32_t score_0_to_100);
void                phoxdrive_trigger_broken_vehicle(uint32_t distance_meters);
void                phoxdrive_clear_broken_vehicle(void);
void                phoxdrive_trigger_accident_zone(bool approaching);

/* Voice / Audio Alert Trigger */
void                phoxdrive_dispatch_voice_alert(const char* message);

#ifdef __cplusplus
}
#endif

#endif /* PHOXDRIVE_H */
