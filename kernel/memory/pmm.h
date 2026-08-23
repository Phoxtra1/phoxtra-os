#ifndef PHOXTRA_PMM_H
#define PHOXTRA_PMM_H

#include <stdint.h>
#include <stddef.h>

#define PAGE_SIZE 4096

#ifdef __cplusplus
extern "C" {
#endif

void pmm_init(uint64_t mem_size);
void* pmm_alloc_page(void);
void pmm_free_page(void* ptr);
uint64_t pmm_get_free_memory(void);
uint64_t pmm_get_total_memory(void);

#ifdef __cplusplus
}
#endif

#endif
