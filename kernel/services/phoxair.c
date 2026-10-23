#include "phoxair.h"
#include "../log/logger.h"
static phoxair_status_t current_phoxair = { .active_targets = 0, .radar_range_km = 0, .telemetry_status = 0 };
void phoxair_init(void) {
    klog(LOG_WARNING, "Phoxair radar/telemetry integration is not connected to a sensor or external data source");
}
phoxair_status_t phoxair_get_status(void) { return current_phoxair; }
void phoxair_scan_airspace(void) {
    klog(LOG_WARNING, "Phoxair scan unavailable: no radar hardware or telemetry feed is configured");
}
