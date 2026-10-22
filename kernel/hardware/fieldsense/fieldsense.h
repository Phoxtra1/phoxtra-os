#ifndef FIELDSENSE_H
#define FIELDSENSE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ──────────────────────────────────────────────────────────
   Phoxtra OS - FieldSense Agricultural Hardware Subsystem
   Groundwork and native runtime for future Phoxtra AgTech &
   soil sensing hardware products powered by Phoxtra OS.
   ────────────────────────────────────────────────────────── */

typedef struct fieldsense_telemetry {
    uint32_t soil_moisture_pct;      /* 0 - 100% */
    int32_t  soil_temp_c;            /* Degrees Celsius */
    uint32_t air_humidity_pct;       /* 0 - 100% */
    uint32_t soil_ph_e2;             /* pH * 100 (e.g. 680 = pH 6.8) */
    uint32_t solar_battery_pct;      /* 0 - 100% */
    bool     irrigation_valve_active;
    bool     drought_stress_alert;
} fieldsense_telemetry_t;

void                    fieldsense_init(void);
fieldsense_telemetry_t* fieldsense_get_telemetry(void);
void                    fieldsense_set_moisture(uint32_t moisture_pct);
void                    fieldsense_trigger_irrigation(bool activate);

#ifdef __cplusplus
}
#endif

#endif /* FIELDSENSE_H */
