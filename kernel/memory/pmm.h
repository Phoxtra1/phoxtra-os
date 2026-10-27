#ifndef PHOXTRA_PMM_H
#define PHOXTRA_PMM_H

#include <stdint.h>
#include <stddef.h>

#define PAGE_SIZE 4096

#ifdef __cplusplus
extern "C" {
#endif

void pmm_set_boot_info(uint32_t magic, uint32_t info_phys);
void pmm_init(uint64_t mem_size);
void* pmm_alloc_page(void);
void* pmm_alloc_user_page(void);
int pmm_is_user_page(uint64_t phys_addr);
int pmm_claim_user_page(uint64_t phys_addr);
void pmm_release_user_mapping(uint64_t phys_addr);
void pmm_free_user_page(void* ptr);
void* pmm_alloc_contiguous_pages(size_t page_count);
void pmm_free_page(void* ptr);
uint64_t pmm_get_free_memory(void);
uint64_t pmm_get_total_memory(void);

#ifdef __cplusplus
}
#endif

#endif
