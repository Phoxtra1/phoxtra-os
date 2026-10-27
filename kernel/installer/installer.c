#include "installer.h"
#include "../log/logger.h"

static installer_step_t current_step = INSTALLER_WELCOME;

void installer_init(void) {
    current_step = INSTALLER_WELCOME;
    klog(LOG_INFO, "Installer disabled: physical disk installation and partition formatting are not implemented");
}

void installer_start(void) {
    current_step = INSTALLER_WELCOME;
    inst_win = compositor_create_window("Phoxtra OS Installer (Preview)", 100, 80, 820, 560, 0x00020617);
    if (!inst_win) return;

    klog(LOG_WARNING, "Installer preview only: disk selection, partitioning, formatting, file copy, and boot installation are not implemented");
}

void installer_next_step(void) {
    /* Do not report installation progress until real disk operations exist. */
    klog(LOG_WARNING, "Installer cannot advance: persistent disk installation is not implemented");
}

installer_step_t installer_get_step(void) {
    return current_step;
}
