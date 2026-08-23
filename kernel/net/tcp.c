#include "tcp.h"
#include "../log/logger.h"

void tcp_init(void) {
    klog(LOG_INFO, "Initializing Transmission Control Protocol (TCP) State Machine...");
}

const char* tcp_state_to_string(tcp_state_t state) {
    switch (state) {
        case TCP_STATE_CLOSED:       return "CLOSED";
        case TCP_STATE_LISTEN:       return "LISTEN";
        case TCP_STATE_SYN_SENT:     return "SYN_SENT";
        case TCP_STATE_SYN_RECEIVED: return "SYN_RECEIVED";
        case TCP_STATE_ESTABLISHED:  return "ESTABLISHED";
        case TCP_STATE_FIN_WAIT:     return "FIN_WAIT";
        default:                     return "UNKNOWN";
    }
}
