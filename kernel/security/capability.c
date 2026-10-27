#include "capability.h"
#include "audit_logger.h"
#include "../log/logger.h"
void capability_init(void) {
    klog(LOG_WARNING, "Capability bitmask helpers are present; privileged syscall and device-operation enforcement is incomplete");
}
int capability_check(pcb_t* pcb, uint64_t cap_flag) {
    if (!pcb || !cap_flag) return 0;
    if ((pcb->capabilities & cap_flag) == cap_flag) return 1;
    const char msg[] = "Capability denied";
    audit_log_event(AUDIT_TYPE_POLICY_VIOLATION, pcb->pid, msg);
    klog(LOG_SECURITY, "Capability check denied an operation");
    return 0;
}
int capability_grant(pcb_t* pcb, uint64_t cap_flag) {
    if (!pcb || !cap_flag) return -1;
    pcb->capabilities |= cap_flag; return 0;
}
int capability_revoke(pcb_t* pcb, uint64_t cap_flag) {
    if (!pcb || !cap_flag) return -1;
    pcb->capabilities &= ~cap_flag; return 0;
}
const char* capability_name(uint64_t flag) {
    switch (flag) {
        case CAP_SYS_ADMIN: return "CAP_SYS_ADMIN";
        case CAP_DEV_ACCESS: return "CAP_DEV_ACCESS";
        case CAP_IPC_SEND: return "CAP_IPC_SEND";
        case CAP_FS_MOUNT: return "CAP_FS_MOUNT";
        case CAP_PROCESS_KILL: return "CAP_PROCESS_KILL";
        case CAP_SECURITY_AUDIT: return "CAP_SECURITY_AUDIT";
        case CAP_NET_SOCKET: return "CAP_NET_SOCKET";
        default: return "CAP_CUSTOM";
    }
}
