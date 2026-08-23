#ifndef PHOXTRA_DHCP_H
#define PHOXTRA_DHCP_H

#include "netdev.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum dhcp_state {
    DHCP_STATE_INIT = 0,
    DHCP_STATE_DISCOVER,
    DHCP_STATE_OFFER,
    DHCP_STATE_REQUEST,
    DHCP_STATE_BOUND
} dhcp_state_t;

void dhcp_init(void);
int dhcp_request_lease(netdev_t* dev);
dhcp_state_t dhcp_get_state(void);

#ifdef __cplusplus
}
#endif

#endif
