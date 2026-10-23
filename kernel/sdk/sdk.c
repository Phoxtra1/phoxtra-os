#include "sdk.h"
#include "../log/logger.h"
void sdk_init(void) {
    klog(LOG_WARNING, "User-space SDK/toolchain is not implemented; the current build system builds the kernel and ISO only");
}
const char* sdk_get_version(void) { return "Phoxtra SDK unavailable in current prototype"; }
uint32_t sdk_get_registered_syscall_count(void) { return 0; }
