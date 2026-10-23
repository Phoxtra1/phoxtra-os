#include "phoxtra_box.h"
#include "../log/logger.h"
void phoxtra_box_init(void) {
    klog(LOG_WARNING, "Phoxtra-Box is a planned integration; Android APK and Windows EXE execution are not implemented");
}
uint32_t phoxtra_box_run_apk(const char* apk_path) {
    if (!apk_path) return 0;
    klog(LOG_WARNING, "APK execution unavailable: no Android runtime or sandbox is implemented");
    return 0;
}
uint32_t phoxtra_box_run_exe(const char* exe_path) {
    if (!exe_path) return 0;
    klog(LOG_WARNING, "EXE execution unavailable: no PE loader or Windows compatibility runtime is implemented");
    return 0;
}
