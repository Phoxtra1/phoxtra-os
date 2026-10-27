#ifndef PHOXTRA_PHOXAIR_H
#define PHOXTRA_PHOXAIR_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t active_targets;
    uint32_t radar_range_km;
    uint32_t telemetry_status;
} phoxair_status_t;

void phoxair_init(void);
phoxair_status_t phoxair_get_status(void);
void phoxair_scan_airspace(void);

#ifdef __cplusplus
}
#endif

#endif // PHOXTRA_PHOXAIR_H
