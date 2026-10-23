#include "dhcp.h"
#include "../log/logger.h"
static dhcp_state_t current_dhcp_state = DHCP_STATE_INIT;
void dhcp_init(void) {
    current_dhcp_state = DHCP_STATE_INIT;
    klog(LOG_WARNING, "DHCP client is not complete: packet receive/offer validation is not implemented");
}
int dhcp_request_lease(netdev_t* dev) {
    if (!dev) return -1;
    current_dhcp_state = DHCP_STATE_FAILED;
    klog(LOG_WARNING, "DHCP lease request unavailable; no address or gateway was fabricated");
    return -2;
}
dhcp_state_t dhcp_get_state(void) { return current_dhcp_state; }
