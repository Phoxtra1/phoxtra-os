#include "phoxair.h"
#include "../log/logger.h"

static phoxair_status_t current_phoxair = { .active_targets = 4, .radar_range_km = 150, .telemetry_status = 1 };

void phoxair_init(void) {
    klog(LOG_INFO, "Initializing Phoxair Airspace & Telemetry Radar Daemon...");
}

phoxair_status_t phoxair_get_status(void) {
    return current_phoxair;
}

void phoxair_scan_airspace(void) {
    klog(LOG_INFO, "Phoxair: Scanning 3D Airspace & Network Telemetry...");
}
