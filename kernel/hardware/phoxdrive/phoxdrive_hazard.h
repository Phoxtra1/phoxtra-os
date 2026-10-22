#ifndef PHOXDRIVE_HAZARD_H
#define PHOXDRIVE_HAZARD_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    HAZARD_TYPE_NONE             = 0,
    HAZARD_TYPE_BROKEN_VEHICLE   = 1,
    HAZARD_TYPE_ACCIDENT_ZONE    = 2,
    HAZARD_TYPE_DEPLORABLE_ROAD  = 3,
    HAZARD_TYPE_OVERSPEED        = 4,
    HAZARD_TYPE_OBSTACLE         = 5
} hazard_type_t;

typedef struct hazard_record {
    uint32_t      hazard_id;
    hazard_type_t type;
    uint32_t      distance_m;
    uint32_t      severity;       /* 1 = Low, 2 = Medium, 3 = Critical */
    char          description[64];
    uint64_t      timestamp_tick;
    bool          active;
} hazard_record_t;

#define MAX_ACTIVE_HAZARDS 8

void              phoxdrive_hazard_init(void);
bool              phoxdrive_register_hazard(hazard_type_t type, uint32_t distance_m, uint32_t severity, const char* desc);
void              phoxdrive_clear_hazard(uint32_t hazard_id);
void              phoxdrive_clear_all_hazards(void);
uint32_t          phoxdrive_get_active_hazard_count(void);
hazard_record_t*  phoxdrive_get_hazard_at(uint32_t index);
const char*       phoxdrive_hazard_type_to_str(hazard_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* PHOXDRIVE_HAZARD_H */
