#include "phoxair.h"
#include "../log/logger.h"

static phoxair_status_t current_phoxair = { .active_targets = 0, .radar_range_km = 0, .telemetry_status = 0 };

void phoxair_init(void) {
    current_phoxair.active_targets = 0;
    current_phoxair.radar_range_km = 0;
    current_phoxair.telemetry_status = 0;
    klog(LOG_WARNING, "Phoxair sensor/radar provider is not connected; measurements are unavailable");
}

phoxair_status_t phoxair_get_status(void) {
    return current_phoxair;
}

void phoxair_scan_airspace(void) {
    klog(LOG_WARNING, "Phoxair scan not performed; no airspace sensor or telemetry provider is implemented");
}
