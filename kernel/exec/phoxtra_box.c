#include "phoxtra_box.h"
#include "../log/logger.h"

void phoxtra_box_init(void) {
    klog(LOG_WARNING, "Phoxtra-Box is a placeholder; APK and EXE execution are unsupported");
}

uint32_t phoxtra_box_run_apk(const char* apk_path) {
    (void)apk_path;
    klog(LOG_WARNING, "APK execution is unsupported; request was not executed");
    return 0;
}

uint32_t phoxtra_box_run_exe(const char* exe_path) {
    (void)exe_path;
    klog(LOG_WARNING, "EXE execution is unsupported; request was not executed");
    return 0;
}
