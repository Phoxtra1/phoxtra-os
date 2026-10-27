#include "installer.h"
#include "../log/logger.h"

static installer_step_t current_step = INSTALLER_WELCOME;

void installer_init(void) {
    current_step = INSTALLER_WELCOME;
    klog(LOG_INFO, "Installer disabled: physical disk installation and partition formatting are not implemented");
}

void installer_start(void) {
    current_step = INSTALLER_WELCOME;
    klog(LOG_WARNING, "Installer disabled: physical disk installation, partition formatting, and file copy are not implemented");

}

void installer_next_step(void) {
    /* Do not report installation progress until real disk operations exist. */
    klog(LOG_WARNING, "Installer cannot advance: persistent disk installation is not implemented");
}

installer_step_t installer_get_step(void) {
    return current_step;
}
