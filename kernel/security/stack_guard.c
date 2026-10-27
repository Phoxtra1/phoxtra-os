#include "stack_guard.h"
#include "tpm2.h"
#include "audit_logger.h"
#include "../log/logger.h"
uint64_t __stack_chk_guard = 0x50484F5854524130ULL;
static uint8_t stack_guard_active = 0;
void stack_guard_init(void) {
    stack_guard_active = 0;
    klog(LOG_WARNING, "Prototype stack canary fallback is present; compiler stack protection and hardware entropy are not enabled");
}
int stack_guard_setup(pcb_t* pcb) {
    if (!pcb || !pcb->stack_base) return -1;
    klog(LOG_WARNING, "Guard-page setup unavailable: stack allocation and per-process page-table ownership are not implemented");
    return -2;
}
void __stack_chk_fail(void) {
    const char msg[] = "Stack Canary Corrupted";
    audit_log_event(AUDIT_TYPE_POLICY_VIOLATION, 0, msg);
    klog(LOG_ERROR, "FATAL SECURITY PANIC: stack canary check failed");
    while (1) asm volatile("cli; hlt");
}
uint64_t aslr_randomize_offset(uint64_t base_address, uint64_t entropy_bits) {
    (void)entropy_bits;
    klog(LOG_WARNING, "ASLR unavailable: no trusted entropy source is configured");
    return base_address;
}
int stack_guard_is_active(void) { return stack_guard_active; }
