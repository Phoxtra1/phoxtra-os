#include "installer.h"
#include "../ui/compositor.h"
#include "../ui/font.h"
#include "../ui/theme.h"
#include "../drivers/framebuffer.h"
#include "../drivers/pci.h"
#include "../log/logger.h"

static window_t* inst_win = 0;
static installer_step_t current_step = INSTALLER_WELCOME;

void installer_init(void) {
    current_step = INSTALLER_WELCOME;
    klog(LOG_INFO, "Initializing Phoxtra Bare-Metal OS Installer Wizard...");
}

void installer_start(void) {
    current_step = INSTALLER_WELCOME;
    inst_win = compositor_create_window("Phoxtra OS v1.0.0 Commercial Installer", 100, 80, 820, 560, 0x00020617);
    if (!inst_win) return;

    klog(LOG_INFO, "Launched Phoxtra OS Installation Wizard");
}

void installer_next_step(void) {
    if (current_step < INSTALLER_COMPLETE) {
        current_step++;
        klog(LOG_INFO, "Installer Advanced to Next Step");
    }
}

installer_step_t installer_get_step(void) {
    return current_step;
}
