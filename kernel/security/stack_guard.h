#ifndef PHOXTRA_STACK_GUARD_H
#define PHOXTRA_STACK_GUARD_H

#include "../process/pcb.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

extern uint64_t __stack_chk_guard;

void stack_guard_init(void);
int stack_guard_setup(pcb_t* pcb);
void __stack_chk_fail(void);
uint64_t aslr_randomize_offset(uint64_t base_address, uint64_t entropy_bits);
int stack_guard_is_active(void);

#ifdef __cplusplus
}
#endif

#endif
