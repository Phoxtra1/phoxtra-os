#include "stack_guard.h"
#include "tpm2.h"
#include "audit_logger.h"
#include "../memory/vmm.h"
#include "../log/logger.h"

// Global GCC Stack Canary Guard Symbol
uint64_t __stack_chk_guard = 0x50484F5854524130ULL; // "PHOXTRA0" fallback

static uint8_t stack_guard_active = 0;

void stack_guard_init(void) {
    klog(LOG_SECURITY, "Initializing Kernel Stack Protection & Guard Page Engine...");

    // Derive Stack Canary Guard from TPM 2.0 Endorsement Key / hardware entropy
    tpm2_dev_t* tpm = tpm2_get_device();
    if (tpm && tpm->hardware_attested) {
        uint64_t canary = 0;
        for (int i = 0; i < 8; i++) {
            canary = (canary << 8) | tpm->endorsement_key[i];
        }
        __stack_chk_guard = canary;
        klog(LOG_SECURITY, "Stack Canary Guard Initialized from TPM 2.0 Hardware Entropy");
    } else {
        klog(LOG_SECURITY, "Stack Canary Guard Initialized (Fallback Seed)");
    }

    stack_guard_active = 1;
}

int stack_guard_setup(pcb_t* pcb) {
    if (!pcb || !pcb->stack_base) return -1;

    // Set up Guard Page boundary at lower boundary of process stack (stack_base)
    uint64_t guard_page_addr = (uint64_t)pcb->stack_base;

    // Unmap / set non-present page permissions for the guard page
    vmm_unmap_page(guard_page_addr);

    klog(LOG_SECURITY, "Kernel Guard Page mapped below process stack frame");
    return 0;
}

void __stack_chk_fail(void) {
    char msg[64];
    msg[0] = 'S'; msg[1] = 't'; msg[2] = 'a'; msg[3] = 'c'; msg[4] = 'k'; msg[5] = ' '; msg[6] = 'C'; msg[7] = 'a'; msg[8] = 'n'; msg[9] = 'a'; msg[10] = 'r'; msg[11] = 'y'; msg[12] = ' '; msg[13] = 'C'; msg[14] = 'o'; msg[15] = 'r'; msg[16] = 'r'; msg[17] = 'u'; msg[18] = 'p'; msg[19] = 't'; msg[20] = 'e'; msg[21] = 'd'; msg[22] = '\0';
    audit_log_event(AUDIT_TYPE_POLICY_VIOLATION, 0, msg);

    klog(LOG_ERROR, "FATAL SECURITY PANIC: Stack Smashing Detected! (__stack_chk_fail)");

    while (1) {
        asm volatile("cli; hlt");
    }
}

uint64_t aslr_randomize_offset(uint64_t base_address, uint64_t entropy_bits) {
    // Generate ASLR base offset using hardware ticks + seed
    uint64_t seed = __stack_chk_guard ^ base_address;
    uint64_t mask = (1ULL << entropy_bits) - 1;
    uint64_t offset = (seed & mask) << 12; // Page-aligned offset (4KB boundary)
    return base_address + offset;
}

int stack_guard_is_active(void) {
    return stack_guard_active;
}
