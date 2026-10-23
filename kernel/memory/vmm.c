#include "vmm.h"
#include "pmm.h"
#include "../log/logger.h"

static uint64_t* pml4_root = NULL;

static inline uint64_t read_cr3(void) {
    uint64_t cr3_val;
    asm volatile("mov %%cr3, %0" : "=r"(cr3_val));
    return cr3_val;
}

static inline void invlpg(void* m) {
    asm volatile("invlpg (%0)" : : "b"(m) : "memory");
}

void vmm_init(void) {
    // Acquire current CR3 table initialized in boot.S
    pml4_root = (uint64_t*)(read_cr3() & ~0xFFFULL);
    
    klog(LOG_KERNEL, "Paging structure validated");
    klog(LOG_KERNEL, "Virtual Memory Manager (VMM) started");
}

int vmm_map_page(uint64_t virt, uint64_t phys, uint32_t flags) {
    uint64_t pml4_idx = (virt >> 39) & 0x1FF;
    uint64_t pdpt_idx = (virt >> 30) & 0x1FF;
    uint64_t pdt_idx  = (virt >> 21) & 0x1FF;
    uint64_t pt_idx   = (virt >> 12) & 0x1FF;

    if (!pml4_root) {
        pml4_root = (uint64_t*)(read_cr3() & ~0xFFF);
    }

    uint64_t* pdpt;
    if (!(pml4_root[pml4_idx] & VMM_FLAG_PRESENT)) {
        pdpt = (uint64_t*)pmm_alloc_page();
        if (!pdpt) return -1;
        for (int i = 0; i < 512; i++) pdpt[i] = 0;
        pml4_root[pml4_idx] = ((uint64_t)pdpt) | VMM_FLAG_PRESENT | VMM_FLAG_WRITABLE | (flags & VMM_FLAG_USER);
    } else {
        pdpt = (uint64_t*)(pml4_root[pml4_idx] & ~0xFFF);
    }

    uint64_t* pdt;
    if (!(pdpt[pdpt_idx] & VMM_FLAG_PRESENT)) {
        pdt = (uint64_t*)pmm_alloc_page();
        if (!pdt) return -1;
        for (int i = 0; i < 512; i++) pdt[i] = 0;
        pdpt[pdpt_idx] = ((uint64_t)pdt) | VMM_FLAG_PRESENT | VMM_FLAG_WRITABLE | (flags & VMM_FLAG_USER);
    } else {
        pdt = (uint64_t*)(pdpt[pdpt_idx] & ~0xFFF);
    }

    uint64_t* pt;
    if (!(pdt[pdt_idx] & VMM_FLAG_PRESENT)) {
        pt = (uint64_t*)pmm_alloc_page();
        if (!pt) return -1;
        for (int i = 0; i < 512; i++) pt[i] = 0;
        pdt[pdt_idx] = ((uint64_t)pt) | VMM_FLAG_PRESENT | VMM_FLAG_WRITABLE | (flags & VMM_FLAG_USER);
    } else {
        pt = (uint64_t*)(pdt[pdt_idx] & ~0xFFF);
    }

    pt[pt_idx] = (phys & ~0xFFF) | (flags & 0xFFF) | VMM_FLAG_PRESENT;
    invlpg((void*)virt);
    return 0;
}

int vmm_unmap_page(uint64_t virt) {
    uint64_t pml4_idx = (virt >> 39) & 0x1FF;
    uint64_t pdpt_idx = (virt >> 30) & 0x1FF;
    uint64_t pdt_idx  = (virt >> 21) & 0x1FF;
    uint64_t pt_idx   = (virt >> 12) & 0x1FF;

    if (!pml4_root || !(pml4_root[pml4_idx] & VMM_FLAG_PRESENT)) return -1;
    uint64_t* pdpt = (uint64_t*)(pml4_root[pml4_idx] & ~0xFFF);
    
    if (!(pdpt[pdpt_idx] & VMM_FLAG_PRESENT)) return -1;
    uint64_t* pdt = (uint64_t*)(pdpt[pdpt_idx] & ~0xFFF);

    if (!(pdt[pdt_idx] & VMM_FLAG_PRESENT)) return -1;
    uint64_t* pt = (uint64_t*)(pdt[pdt_idx] & ~0xFFF);

    pt[pt_idx] = 0;
    invlpg((void*)virt);
    return 0;
}

uint64_t vmm_virt_to_phys(uint64_t virt) {
    uint64_t pml4_idx = (virt >> 39) & 0x1FF;
    uint64_t pdpt_idx = (virt >> 30) & 0x1FF;
    uint64_t pdt_idx  = (virt >> 21) & 0x1FF;
    uint64_t pt_idx   = (virt >> 12) & 0x1FF;

    if (!pml4_root || !(pml4_root[pml4_idx] & VMM_FLAG_PRESENT)) return 0;
    uint64_t* pdpt = (uint64_t*)(pml4_root[pml4_idx] & ~0xFFF);

    if (!(pdpt[pdpt_idx] & VMM_FLAG_PRESENT)) return 0;
    uint64_t* pdt = (uint64_t*)(pdpt[pdpt_idx] & ~0xFFF);

    if (!(pdt[pdt_idx] & VMM_FLAG_PRESENT)) return 0;

    // Check if huge page (2MB)
    if (pdt[pdt_idx] & 0x80) {
        return (pdt[pdt_idx] & ~0x1FFFFF) + (virt & 0x1FFFFF);
    }

    uint64_t* pt = (uint64_t*)(pdt[pdt_idx] & ~0xFFF);
    if (!(pt[pt_idx] & VMM_FLAG_PRESENT)) return 0;

    return (pt[pt_idx] & ~0xFFF) + (virt & 0xFFF);
}

uint64_t vmm_get_kernel_page_directory(void) {
    if (!pml4_root) {
        pml4_root = (uint64_t*)(read_cr3() & ~0xFFFULL);
    }
    return (uint64_t)pml4_root;
}

uint64_t vmm_create_user_page_directory(void) {
    uint64_t* user_pml4 = (uint64_t*)pmm_alloc_page();
    if (!user_pml4) {
        klog(LOG_ERROR, "VMM: Failed to allocate physical page for user PML4");
        return 0;
    }

    uint64_t k_cr3 = vmm_get_kernel_page_directory();
    uint64_t* k_pml4 = (uint64_t*)k_cr3;

    // Copy identity kernel mappings so kernel code, stacks & hardware memory stay accessible
    for (int i = 0; i < 512; i++) {
        user_pml4[i] = k_pml4[i];
    }

    return (uint64_t)user_pml4;
}

void vmm_switch_page_directory(uint64_t cr3_phys) {
    if (!cr3_phys) return;
    uint64_t current_cr3 = read_cr3() & ~0xFFFULL;
    if (current_cr3 != (cr3_phys & ~0xFFFULL)) {
        asm volatile("mov %0, %%cr3" : : "r"(cr3_phys & ~0xFFFULL) : "memory");
    }
}

int vmm_map_user_page(uint64_t pml4_phys, uint64_t virt, uint64_t phys, uint32_t flags) {
    if (!pml4_phys) return -1;
    /* Ring 3 may only map lower-half canonical virtual addresses. */
    if (virt >= 0x0000800000000000ULL) return -1;
    uint64_t* target_pml4 = (uint64_t*)pml4_phys;
    uint64_t* kernel_pml4 = (uint64_t*)vmm_get_kernel_page_directory();
    if (!target_pml4 || target_pml4 == kernel_pml4) {
        klog(LOG_WARNING, "VMM: refusing user mapping in the kernel page directory");
        return -1;
    }

    uint64_t pml4_idx = (virt >> 39) & 0x1FF;
    uint64_t pdpt_idx = (virt >> 30) & 0x1FF;
    uint64_t pdt_idx  = (virt >> 21) & 0x1FF;
    uint64_t pt_idx   = (virt >> 12) & 0x1FF;

    uint32_t user_flags = flags | VMM_FLAG_PRESENT | VMM_FLAG_USER;

    uint64_t* pdpt;
    if (!(target_pml4[pml4_idx] & VMM_FLAG_PRESENT)) {
        pdpt = (uint64_t*)pmm_alloc_page();
        if (!pdpt) return -1;
        for (int i = 0; i < 512; i++) pdpt[i] = 0;
        target_pml4[pml4_idx] = ((uint64_t)pdpt) | user_flags | VMM_FLAG_WRITABLE;
    } else {
        /* This PML4 entry may be shared with the kernel address space.
           Never make a shared supervisor mapping user-accessible. */
        if (!(target_pml4[pml4_idx] & VMM_FLAG_USER)) {
            klog(LOG_WARNING, "VMM: refusing user mapping in shared supervisor PML4 slot");
            return -1;
        }
        pdpt = (uint64_t*)(target_pml4[pml4_idx] & ~0xFFF);
    }

    uint64_t* pdt;
    if (!(pdpt[pdpt_idx] & VMM_FLAG_PRESENT)) {
        pdt = (uint64_t*)pmm_alloc_page();
        if (!pdt) return -1;
        for (int i = 0; i < 512; i++) pdt[i] = 0;
        pdpt[pdpt_idx] = ((uint64_t)pdt) | user_flags | VMM_FLAG_WRITABLE;
    } else {
        if (!(pdpt[pdpt_idx] & VMM_FLAG_USER)) {
            klog(LOG_WARNING, "VMM: refusing user mapping through shared supervisor PDPT slot");
            return -1;
        }
        pdt = (uint64_t*)(pdpt[pdpt_idx] & ~0xFFF);
    }

    uint64_t* pt;
    if (!(pdt[pdt_idx] & VMM_FLAG_PRESENT)) {
        pt = (uint64_t*)pmm_alloc_page();
        if (!pt) return -1;
        for (int i = 0; i < 512; i++) pt[i] = 0;
        pdt[pdt_idx] = ((uint64_t)pt) | user_flags | VMM_FLAG_WRITABLE;
    } else {
        if (!(pdt[pdt_idx] & VMM_FLAG_USER)) {
            klog(LOG_WARNING, "VMM: refusing user mapping through shared supervisor PD slot");
            return -1;
        }
        pt = (uint64_t*)(pdt[pdt_idx] & ~0xFFF);
    }

    pt[pt_idx] = (phys & ~0xFFF) | user_flags;
    invlpg((void*)virt);
    return 0;
}



/* Bootstrap paging identity-maps physical memory from 0 through 4 GiB. */
int vmm_ensure_identity_mapped(uint64_t phys_start, uint64_t size) {
    if (!size || phys_start >= 0x100000000ULL ||
        size > 0x100000000ULL - phys_start) return -1;
    uint64_t last = phys_start + size - 1;
    if (vmm_virt_to_phys(phys_start) != phys_start ||
        vmm_virt_to_phys(last) != last) return -1;
    return 0;
}


/* Mark the 2 MiB identity-mapped page(s) containing MMIO framebuffer memory
   uncacheable. The bootstrap uses huge pages, so set PCD/PWT on those PDEs. */
int vmm_mark_uncached(uint64_t phys_start, uint64_t size) {
    if (!size || phys_start >= 0x100000000ULL ||
        size > 0x100000000ULL - phys_start) return -1;
    uint64_t first = phys_start & ~0x1FFFFFULL;
    uint64_t last = (phys_start + size - 1) & ~0x1FFFFFULL;

    /* Flush cached data before changing the memory type of the mapping. */
    asm volatile("wbinvd" ::: "memory");
    for (uint64_t base = first; ; base += 0x200000ULL) {
        uint64_t pml4_idx = (base >> 39) & 0x1FF;
        uint64_t pdpt_idx = (base >> 30) & 0x1FF;
        uint64_t pdt_idx  = (base >> 21) & 0x1FF;
        if (!pml4_root || !(pml4_root[pml4_idx] & VMM_FLAG_PRESENT)) return -1;
        uint64_t* pdpt = (uint64_t*)(pml4_root[pml4_idx] & ~0xFFFULL);
        if (!(pdpt[pdpt_idx] & VMM_FLAG_PRESENT)) return -1;
        uint64_t* pdt = (uint64_t*)(pdpt[pdpt_idx] & ~0xFFFULL);
        if (!(pdt[pdt_idx] & VMM_FLAG_PRESENT) || !(pdt[pdt_idx] & 0x80ULL)) return -1;
        pdt[pdt_idx] |= 0x18ULL; /* PWT | PCD: uncached framebuffer access */
        invlpg((void*)base);
        if (base == last) break;
    }
    asm volatile("wbinvd" ::: "memory");
    return 0;
}
