#include "continuum.h"
#include "../log/logger.h"

static continuum_mode_t current_mode = CONTINUUM_MODE_DESKTOP;

void continuum_init(void) {
    klog(LOG_INFO, "Initializing Phoxtra Continuum USB-C DisplayPort Docking Subsystem...");
}

continuum_mode_t continuum_get_mode(void) {
    return current_mode;
}

void continuum_switch_mode(continuum_mode_t mode) {
    current_mode = mode;
    if (mode == CONTINUUM_MODE_DESKTOP) {
        klog(LOG_INFO, "Phoxtra Continuum: USB-C Monitor Detected -> Switched to Desktop OS Mode");
    } else {
        klog(LOG_INFO, "Phoxtra Continuum: Mobile Dock Disconnected -> Switched to Mobile Touch Mode");
    }
}
