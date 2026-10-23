#include "installer.h"
#include "../log/logger.h"

static installer_step_t current_step = INSTALLER_WELCOME;

void installer_init(void) {
    current_step = INSTALLER_WELCOME;
    klog(LOG_INFO, "Installer disabled: physical disk installation and partition formatting are not implemented");
}

void installer_start(void) {
    current_step = INSTALLER_WELCOME;
    klog(LOG_WARNING, "Installer disabled: use the bootable test environment; no disk writes are supported");
}

void installer_next_step(void) {
    /* Fail closed: no installer step can advance to disk selection or formatting. */
    current_step = INSTALLER_WELCOME;
}

installer_step_t installer_get_step(void) {
    return current_step;
}
