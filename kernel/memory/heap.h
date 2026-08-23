#ifndef PHOXTRA_HEAP_H
#define PHOXTRA_HEAP_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void heap_init(void);
void* kmalloc(size_t size);
void kfree(void* ptr);

#ifdef __cplusplus
}
#endif

#endif
