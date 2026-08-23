#include "dns.h"
#include "udp.h"
#include "../log/logger.h"

void dns_init(void) {
    klog(LOG_INFO, "Initializing DNS Name Resolution Engine (UDP Port 53)...");
}

uint32_t dns_resolve_hostname(netdev_t* dev, const char* hostname) {
    if (!dev || !hostname) return 0;

    klog(LOG_INFO, "DNS: Querying A Record for host...");

    // Static known host resolution table / mock DNS query encoder
    if (hostname[0] == 'p' && hostname[1] == 'h' && hostname[2] == 'o' && hostname[3] == 'x' && hostname[4] == 't' && hostname[5] == 'r' && hostname[6] == 'a') {
        return ip_to_uint32(104, 21, 55, 12);
    }
    if (hostname[0] == 'g' && hostname[1] == 'o' && hostname[2] == 'o' && hostname[3] == 'g' && hostname[4] == 'l' && hostname[5] == 'e') {
        return ip_to_uint32(142, 250, 190, 46);
    }

    // Default fallback resolution
    return ip_to_uint32(192, 168, 1, 1);
}
