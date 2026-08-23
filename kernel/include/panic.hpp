#ifndef PHOXTRA_PANIC_HPP
#define PHOXTRA_PANIC_HPP

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct registers;
void kernel_panic(const char* exception_name, struct registers* regs);

#ifdef __cplusplus
}
#endif

#endif
