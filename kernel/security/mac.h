#ifndef PHOXTRA_MAC_H
#define PHOXTRA_MAC_H

#include "../process/pcb.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAC_LABEL_SYSTEM     0  /* Kernel Core & Master Drivers */
#define MAC_LABEL_SERVICE    1  /* Trusted System Services */
#define MAC_LABEL_USER       2  /* Ring 3 User Applications */
#define MAC_LABEL_RESTRICTED 3  /* Restricted Sandbox Process */

void mac_init(void);
int mac_check_vfs_access(pcb_t* pcb, uint32_t node_mac_label, uint32_t mode);
int mac_check_ipc_access(pcb_t* sender, pcb_t* receiver);
const char* mac_label_to_string(uint32_t label);

#ifdef __cplusplus
}
#endif

#endif
