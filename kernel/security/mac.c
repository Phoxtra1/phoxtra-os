#include "mac.h"
#include "audit_logger.h"
#include "../log/logger.h"

void mac_init(void) {
    klog(LOG_SECURITY, "Initializing Mandatory Access Control (MAC) Policy Framework...");
    klog(LOG_WARNING, "MAC policy helpers exist, but VFS and IPC enforcement are not wired end-to-end");
}

int mac_check_vfs_access(pcb_t* pcb, uint32_t node_mac_label, uint32_t mode) {
    if (!pcb) return 0;

    // Rule 1: SYSTEM context has full access
    if (pcb->mac_label == MAC_LABEL_SYSTEM) return 1;

    // Rule 2: RESTRICTED sandbox cannot write to non-restricted objects
    if (pcb->mac_label == MAC_LABEL_RESTRICTED && node_mac_label != MAC_LABEL_RESTRICTED) {
        char msg[64];
        msg[0] = 'M'; msg[1] = 'A'; msg[2] = 'C'; msg[3] = ' '; msg[4] = 'S'; msg[5] = 'a'; msg[6] = 'n'; msg[7] = 'd'; msg[8] = 'b'; msg[9] = 'o'; msg[10] = 'x'; msg[11] = '\0';
        audit_log_event(AUDIT_TYPE_POLICY_VIOLATION, pcb->pid, msg);
        klog(LOG_SECURITY, "MAC VIOLATION: Sandbox process attempted access out of domain");
        return 0; // Access Denied
    }

    // Rule 3: USER context cannot write to SYSTEM or SERVICE objects
    if (pcb->mac_label == MAC_LABEL_USER && (node_mac_label == MAC_LABEL_SYSTEM || node_mac_label == MAC_LABEL_SERVICE)) {
        if (mode & 0x02) { // Write attempt
            char msg[64];
            msg[0] = 'M'; msg[1] = 'A'; msg[2] = 'C'; msg[3] = ' '; msg[4] = 'W'; msg[5] = 'r'; msg[6] = 'i'; msg[7] = 't'; msg[8] = 'e'; msg[9] = ' '; msg[10] = 'D'; msg[11] = 'e'; msg[12] = 'n'; msg[13] = 'i'; msg[14] = 'e'; msg[15] = 'd'; msg[16] = '\0';
            audit_log_event(AUDIT_TYPE_POLICY_VIOLATION, pcb->pid, msg);
            klog(LOG_SECURITY, "MAC VIOLATION: User process attempted write to System domain");
            return 0;
        }
    }

    return 1; // Access Granted
}

int mac_check_ipc_access(pcb_t* sender, pcb_t* receiver) {
    if (!sender || !receiver) return 0;

    // SYSTEM and SERVICE can transmit IPC to any domain
    if (sender->mac_label <= MAC_LABEL_SERVICE) return 1;

    // RESTRICTED processes cannot initiate IPC to higher domains
    if (sender->mac_label == MAC_LABEL_RESTRICTED && receiver->mac_label != MAC_LABEL_RESTRICTED) {
        char msg[64];
        msg[0] = 'M'; msg[1] = 'A'; msg[2] = 'C'; msg[3] = ' '; msg[4] = 'I'; msg[5] = 'P'; msg[6] = 'C'; msg[7] = ' '; msg[8] = 'B'; msg[9] = 'l'; msg[10] = 'o'; msg[11] = 'c'; msg[12] = 'k'; msg[13] = '\0';
        audit_log_event(AUDIT_TYPE_POLICY_VIOLATION, sender->pid, msg);
        klog(LOG_SECURITY, "MAC VIOLATION: IPC message blocked between isolation domains");
        return 0;
    }

    return 1;
}

const char* mac_label_to_string(uint32_t label) {
    switch (label) {
        case MAC_LABEL_SYSTEM:     return "SYSTEM";
        case MAC_LABEL_SERVICE:    return "SERVICE";
        case MAC_LABEL_USER:       return "USER";
        case MAC_LABEL_RESTRICTED: return "RESTRICTED";
        default:                   return "UNKNOWN";
    }
}
