#include "guest_https_image.h"
#include "phoxtra_guest_https_elf.h"
#include "process/process.h"
#include "log/logger.h"

void guest_https_smoke_start(void)
{
    const uint8_t* image = phoxtra_guest_https_elf_bytes();
    const size_t size = phoxtra_guest_https_elf_size();
    if (!image || !size ||
        !process_create_user_from_elf("guest-https-smoke", image, (uint64_t)size, 1u)) {
        klog(LOG_ERROR, "PHOXTRA_GUEST_HTTPS_SETUP_FAILED");
        return;
    }
    klog(LOG_INFO, "PHOXTRA_GUEST_HTTPS_PROCESS_QUEUED");
}
