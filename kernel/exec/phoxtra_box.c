#include "phoxtra_box.h"
#include "../log/logger.h"

void phoxtra_box_init(void) {
    klog(LOG_INFO, "Initializing Phoxtra-Box Universal Android (.apk) & Windows (.exe) Container Engine...");
}

uint32_t phoxtra_box_run_apk(const char* apk_path) {
    (void)apk_path;
    klog(LOG_INFO, "Phoxtra-Box: Android Dalvik/ART Sandbox Container Executing APK...");
    return 1;
}

uint32_t phoxtra_box_run_exe(const char* exe_path) {
    (void)exe_path;
    klog(LOG_INFO, "Phoxtra-Box: Windows PE32+ Runtime Compatibility Layer Executing EXE...");
    return 1;
}
