#ifndef PHOXTRA_DNS_H
#define PHOXTRA_DNS_H

#include "netdev.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void dns_init(void);
uint32_t dns_resolve_hostname(netdev_t* dev, const char* hostname);

#ifdef __cplusplus
}
#endif

#endif
