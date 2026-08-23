#ifndef PHOXTRA_PROCESS_H
#define PHOXTRA_PROCESS_H

#include "pcb.h"
#include <stdint.h>

#define MAX_PROCESSES 64

#ifdef __cplusplus
extern "C" {
#endif

void        process_init(void);
pcb_t*      process_create(const char* name, void (*entry)(void), uint32_t priority);
void        process_destroy(uint32_t pid);
void        process_pause(uint32_t pid);
void        process_resume(uint32_t pid);
pcb_t*      process_lookup(uint32_t pid);
uint32_t    process_count(void);
pcb_t**     process_table(void);

#ifdef __cplusplus
}
#endif

#endif
