#ifndef PHOXTRA_SDK_H
#define PHOXTRA_SDK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void sdk_init(void);
const char* sdk_get_version(void);
uint32_t sdk_get_registered_syscall_count(void);

#ifdef __cplusplus
}
#endif

#endif
