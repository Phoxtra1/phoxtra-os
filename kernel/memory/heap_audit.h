#ifndef PHOXTRA_HEAP_AUDIT_H
#define PHOXTRA_HEAP_AUDIT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void heap_audit_init(void);
int heap_audit_verify_allocations(void);
uint32_t heap_audit_get_used_bytes(void);

#ifdef __cplusplus
}
#endif

#endif
