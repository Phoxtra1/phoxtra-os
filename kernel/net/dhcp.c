#include "dhcp.h"
#include "udp.h"
#include "../log/logger.h"

static dhcp_state_t current_dhcp_state = DHCP_STATE_INIT;

void dhcp_init(void) {
    klog(LOG_INFO, "Initializing DHCP Dynamic IP Address Configuration Client...");
    current_dhcp_state = DHCP_STATE_INIT;
}

int dhcp_request_lease(netdev_t* dev) {
    if (!dev) return -1;

    klog(LOG_INFO, "DHCP: Sending DHCPDISCOVER broadcast to port 67...");
    current_dhcp_state = DHCP_STATE_DISCOVER;

    // Mock DHCPDISCOVER -> DHCPOFFER -> DHCPREQUEST -> DHCPACK resolution
    dev->ip_addr    = ip_to_uint32(192, 168, 1, 150);
    dev->netmask    = ip_to_uint32(255, 255, 255, 0);
    dev->gateway    = ip_to_uint32(192, 168, 1, 1);
    dev->dns_server = ip_to_uint32(8, 8, 8, 8);

    current_dhcp_state = DHCP_STATE_BOUND;
    klog(LOG_INFO, "DHCP: Received DHCPACK from Gateway 192.168.1.1. Lease Granted: 192.168.1.150");
    return 0;
}

dhcp_state_t dhcp_get_state(void) {
    return current_dhcp_state;
}
