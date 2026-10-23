#include "dns.h"
#include "../log/logger.h"
void dns_init(void) {
    klog(LOG_WARNING, "DNS resolver is not complete: DNS query/response parsing is not implemented");
}
uint32_t dns_resolve_hostname(netdev_t* dev, const char* hostname) {
    if (!dev || !hostname || !hostname[0]) return 0;
    klog(LOG_WARNING, "DNS lookup unavailable; refusing to return a fabricated IP address");
    return 0;
}
