#ifndef PHOXTRA_PROCESS_IDENTITY_H
#define PHOXTRA_PROCESS_IDENTITY_H

#include "../process/pcb.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define IDENTITY_STATUS_UNVERIFIED 0
#define IDENTITY_STATUS_VERIFIED   1
#define IDENTITY_STATUS_REJECTED   2

void process_identity_init(void);
int process_identity_bind(pcb_t* pcb, const uint8_t* code_ptr, size_t code_len);
int process_identity_verify(pcb_t* pcb);
void process_identity_get_hash_hex(pcb_t* pcb, char hex_out[65]);

#ifdef __cplusplus
}
#endif

#endif
