#include "sdk.h"
#include "../log/logger.h"

void sdk_init(void) {
    klog(LOG_INFO, "Initializing Phoxtra OS Developer SDK Subsystem & Syscall Gateway v1.0...");
}

const char* sdk_get_version(void) {
    return "Phoxtra-SDK v1.0.0-RC3 (x86_64-phoxtra-elf)";
}

uint32_t sdk_get_registered_syscall_count(void) {
    return 64; // 64 Registered POSIX & Security System Calls
}
