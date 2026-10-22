#ifndef HARDWARE_MANAGER_H
#define HARDWARE_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ──────────────────────────────────────────────────────────
   Phoxtra Universal Hardware Device Profile Classification
   Phoxtra OS is the native foundation for all Phoxtra
   hardware products, adapting its runtime profile dynamically.
   ────────────────────────────────────────────────────────── */
typedef enum {
    HARDWARE_CLASS_GENERIC        = 0,
    HARDWARE_CLASS_AUTOMOTIVE     = 1, /* In-vehicle smart systems (Phoxdrive) */
    HARDWARE_CLASS_AGRICULTURE    = 2, /* AgTech & soil sensors (FieldSense)  */
    HARDWARE_CLASS_SMART_MOBILITY = 3, /* Drones, e-mobility, robotic delivery */
    HARDWARE_CLASS_INDUSTRIAL_IOT = 4, /* Factory, environmental & energy grid */
    HARDWARE_CLASS_EDGE_GATEWAY   = 5, /* Distributed telemetry & edge compute */
    HARDWARE_CLASS_MOBILE         = 6, /* Smartphones and tablets              */
    HARDWARE_CLASS_DESKTOP_LAPTOP = 7  /* Personal computers & workstations    */
} hardware_class_t;

typedef enum {
    HARDWARE_STATUS_OFFLINE      = 0,
    HARDWARE_STATUS_INITIALIZING = 1,
    HARDWARE_STATUS_ONLINE       = 2,
    HARDWARE_STATUS_ALERT        = 3,
    HARDWARE_STATUS_DEGRADED     = 4
} hardware_status_t;

#define MAX_HARDWARE_DEVICES 16

typedef struct phoxtra_device {
    uint32_t           device_id;
    char               name[32];
    char               serial[32];
    hardware_class_t   device_class;
    hardware_status_t  status;
    uint32_t           firmware_rev;
    bool               telemetry_active;
    uint64_t           last_heartbeat_tick;
    void (*on_telemetry_event)(const char* event_name, const void* payload);
    void (*on_alert_triggered)(uint32_t alert_level, const char* message);
} phoxtra_device_t;

/* Manager API */
void                hardware_manager_init(void);
bool                hardware_register_device(phoxtra_device_t* dev);
phoxtra_device_t*   hardware_get_device(uint32_t device_id);
phoxtra_device_t*   hardware_get_device_by_name(const char* name);
uint32_t            hardware_device_count(void);
phoxtra_device_t*   hardware_get_device_at(uint32_t index);
const char*         hardware_class_to_string(hardware_class_t dev_class);
const char*         hardware_status_to_string(hardware_status_t status);
hardware_class_t    hardware_get_active_primary_class(void);
void                hardware_set_active_primary_class(hardware_class_t dev_class);

#ifdef __cplusplus
}
#endif

#endif /* HARDWARE_MANAGER_H */
