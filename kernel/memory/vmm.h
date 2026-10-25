#ifndef PHOXTRA_VMM_H
#define PHOXTRA_VMM_H

#include <stdint.h>
#include <stddef.h>

#define VMM_FLAG_PRESENT  0x1u
#define VMM_FLAG_WRITABLE 0x2u
#define VMM_FLAG_USER     0x4u

#ifdef __cplusplus
extern "C" {
#endif

void vmm_init(void);
int vmm_map_page(uint64_t virt, uint64_t phys, uint32_t flags);
int vmm_ensure_identity_mapped(uint64_t phys_start, uint64_t size);
int vmm_mark_uncached(uint64_t phys_start, uint64_t size);
int vmm_unmap_page(uint64_t virt);
uint64_t vmm_virt_to_phys(uint64_t virt);

/*
 * New address spaces keep private lower-half page tables and share only the
 * supervisor-only upper PML4 half. The current boot map identity-maps 0..4 GiB
 * with huge pages, so user mappings are restricted to virtual addresses >=4
 * GiB and below the canonical lower-half limit. Physical frames must be
 * allocated by the caller and remain caller-owned until explicitly released;
 * destroying an address space frees only its page-table pages.
 */
uint64_t vmm_create_user_page_directory(void);
int vmm_destroy_user_page_directory(uint64_t pml4_phys);
void vmm_switch_page_directory(uint64_t cr3_phys);
uint64_t vmm_get_kernel_page_directory(void);
int vmm_map_user_page(uint64_t pml4_phys, uint64_t virt, uint64_t phys, uint32_t flags);

#ifdef __cplusplus
}
#endif

#endif
