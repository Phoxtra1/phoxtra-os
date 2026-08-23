#ifndef PHOXTRA_CAPABILITY_H
#define PHOXTRA_CAPABILITY_H

#include "../process/pcb.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Fine-grained Capability Flags (Replacing legacy UID/GID root) */
#define CAP_NONE           0x00000000ULL
#define CAP_SYS_ADMIN      0x00000001ULL  /* Kernel system administration */
#define CAP_DEV_ACCESS     0x00000002ULL  /* Direct hardware/devfs access */
#define CAP_IPC_SEND       0x00000004ULL  /* Inter-process communication */
#define CAP_FS_MOUNT       0x00000008ULL  /* Mount/unmount filesystems */
#define CAP_PROCESS_KILL   0x00000010ULL  /* Terminate processes */
#define CAP_SECURITY_AUDIT 0x00000020ULL  /* Modify security & audit logs */
#define CAP_NET_SOCKET     0x00000040ULL  /* Network socket creation */
#define CAP_ALL            0xFFFFFFFFULL

void capability_init(void);
int capability_check(pcb_t* pcb, uint64_t cap_flag);
int capability_grant(pcb_t* pcb, uint64_t cap_flag);
int capability_revoke(pcb_t* pcb, uint64_t cap_flag);
const char* capability_name(uint64_t cap_flag);

#ifdef __cplusplus
}
#endif

#endif
