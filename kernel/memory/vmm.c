#include "vmm.h"
#include "pmm.h"
#include "../log/logger.h"
#include "../sync/spinlock.h"

#define VMM_ADDRESS_MASK       0x000FFFFFFFFFF000ULL
#define VMM_PTE_PRESENT        0x001ULL
#define VMM_PTE_WRITABLE       0x002ULL
#define VMM_PTE_USER           0x004ULL
#define VMM_PTE_WRITE_THROUGH  0x008ULL
#define VMM_PTE_CACHE_DISABLE  0x010ULL
#define VMM_PTE_LARGE          0x080ULL
#define VMM_PTE_NO_EXECUTE     (1ULL << 63)
#define VMM_LARGE_PAGE_MASK    0x000FFFFFFFE00000ULL
#define VMM_GIGA_PAGE_MASK     0x000FFFFFC0000000ULL
#define VMM_LOWER_HALF_ENTRIES 256u
#define VMM_USER_MIN_ADDRESS   0x0000000100000000ULL
#define VMM_USER_MAX_ADDRESS   0x0000800000000000ULL
#define VMM_PHYSICAL_LIMIT     0x0000000100000000ULL
#define VMM_RESERVED_PHYSICAL  0x0000000000400000ULL

static uint64_t* kernel_pml4_root;
static uint64_t* active_pml4_root;
static int nx_supported;
#ifndef VMM_TESTING
static spinlock_t vmm_lock = SPINLOCK_INITIALIZER;
#endif
#define VMM_MAX_ADDRESS_SPACES 256u
static uint64_t owned_address_spaces[VMM_MAX_ADDRESS_SPACES];

#ifdef VMM_TESTING
extern uint64_t vmm_test_read_cr3(void);
extern void vmm_test_write_cr3(uint64_t value);
extern void vmm_test_invlpg(uint64_t address);
#endif

static inline uint64_t read_cr3(void) {
#ifdef VMM_TESTING
    return vmm_test_read_cr3();
#else
    uint64_t value;
    __asm__ volatile("mov %%cr3, %0" : "=r"(value));
    return value;
#endif
}

static inline void write_cr3(uint64_t value) {
#ifdef VMM_TESTING
    vmm_test_write_cr3(value);
#else
    __asm__ volatile("mov %0, %%cr3" : : "r"(value) : "memory");
#endif
}

static inline void invlpg(void* address) {
#ifdef VMM_TESTING
    vmm_test_invlpg((uint64_t)(uintptr_t)address);
#else
    __asm__ volatile("invlpg (%0)" : : "b"(address) : "memory");
#endif
}

static inline spinlock_irq_flags_t vmm_lock_acquire(void) {
#ifdef VMM_TESTING
    return 0;
#else
    return spin_lock_irqsave(&vmm_lock);
#endif
}

static inline void vmm_lock_release(spinlock_irq_flags_t flags) {
#ifdef VMM_TESTING
    (void)flags;
#else
    spin_unlock_irqrestore(&vmm_lock, flags);
#endif
}

static inline int is_page_aligned(uint64_t value) {
    return (value & (PAGE_SIZE - 1ULL)) == 0;
}

static inline int is_lower_canonical(uint64_t address) {
    return address < VMM_USER_MAX_ADDRESS;
}

static inline uint64_t* entry_table(uint64_t entry) {
    return (uint64_t*)(uintptr_t)(entry & VMM_ADDRESS_MASK);
}

static inline uint64_t table_entry(const void* table, uint64_t flags) {
    return ((uint64_t)(uintptr_t)table & VMM_ADDRESS_MASK) | flags;
}

static uint64_t* allocate_zeroed_table(void) {
    uint64_t* table = (uint64_t*)pmm_alloc_page();
    if (!table) return NULL;
    for (size_t i = 0; i < 512; ++i) table[i] = 0;
    return table;
}

static uint64_t* current_pml4_unlocked(void) {
    uint64_t* hardware_root = (uint64_t*)(uintptr_t)(read_cr3() & VMM_ADDRESS_MASK);
    if (!kernel_pml4_root) kernel_pml4_root = hardware_root;
    if (!active_pml4_root || active_pml4_root != hardware_root)
        active_pml4_root = hardware_root;
    return active_pml4_root;
}

static uint64_t* kernel_pml4_unlocked(void) {
    if (!kernel_pml4_root) {
        uint64_t* current = (uint64_t*)(uintptr_t)(read_cr3() & VMM_ADDRESS_MASK);
        kernel_pml4_root = current;
        if (!active_pml4_root) active_pml4_root = current;
    }
    return kernel_pml4_root;
}

static int address_space_slot_unlocked(uint64_t pml4_phys) {
    for (size_t i = 0; i < VMM_MAX_ADDRESS_SPACES; ++i)
        if (owned_address_spaces[i] == pml4_phys) return (int)i;
    return -1;
}

static int register_address_space_unlocked(uint64_t pml4_phys) {
    for (size_t i = 0; i < VMM_MAX_ADDRESS_SPACES; ++i) {
        if (!owned_address_spaces[i]) {
            owned_address_spaces[i] = pml4_phys;
            return 0;
        }
    }
    return -1;
}

/*
 * Address-space lower-half page tables are fully private. Leaf PTEs retain
 * their physical-frame references; ownership of those frames remains with the
 * mapper. The upper PML4 half is shared and is therefore never freed here.
 */
static int destroy_page_directory_unlocked(uint64_t pml4_phys) {
    if (!pml4_phys || !is_page_aligned(pml4_phys)) return -1;

    uint64_t current_cr3 = read_cr3() & VMM_ADDRESS_MASK;
    uint64_t* kernel_root = kernel_pml4_unlocked();
    if (pml4_phys == (uint64_t)(uintptr_t)kernel_root ||
        pml4_phys == current_cr3) {
        return -1;
    }

    uint64_t* root = (uint64_t*)(uintptr_t)pml4_phys;
    for (size_t pml4_i = 0; pml4_i < VMM_LOWER_HALF_ENTRIES; ++pml4_i) {
        uint64_t pml4e = root[pml4_i];
        if (!(pml4e & VMM_PTE_PRESENT)) continue;
        uint64_t* pdpt = entry_table(pml4e);

        for (size_t pdpt_i = 0; pdpt_i < 512; ++pdpt_i) {
            uint64_t pdpte = pdpt[pdpt_i];
            if (!(pdpte & VMM_PTE_PRESENT) || (pdpte & VMM_PTE_LARGE))
                continue; /* Includes 1 GiB leaf mappings. */

            uint64_t* pd = entry_table(pdpte);
            for (size_t pd_i = 0; pd_i < 512; ++pd_i) {
                uint64_t pde = pd[pd_i];
                if (!(pde & VMM_PTE_PRESENT) || (pde & VMM_PTE_LARGE))
                    continue; /* A 2 MiB leaf has no PT to release. */
                uint64_t* pt = entry_table(pde);
                for (size_t pt_i = 0; pt_i < 512; ++pt_i) {
                    uint64_t pte = pt[pt_i];
                    if (!(pte & VMM_PTE_PRESENT) || !(pte & VMM_PTE_USER)) continue;
                    uint64_t frame = pte & VMM_ADDRESS_MASK;
                    pmm_release_user_mapping(frame);
                    pmm_free_user_page((void*)(uintptr_t)frame);
                    pt[pt_i] = 0;
                }
                pmm_free_page(pt);
            }
            pmm_free_page(pd);
        }
        pmm_free_page(pdpt);
        root[pml4_i] = 0;
    }

    pmm_free_page(root);
    return 0;
}

/* Called while vmm_lock is held and before the root becomes current. */
static uint64_t clone_kernel_lower_half(void) {
    uint64_t* kernel_root = kernel_pml4_unlocked();
    uint64_t* new_root = allocate_zeroed_table();
    if (!new_root) return 0;

    /* Upper-half kernel mappings stay supervisor-only and shared. */
    for (size_t i = VMM_LOWER_HALF_ENTRIES; i < 512; ++i)
        new_root[i] = kernel_root[i] & ~VMM_PTE_USER;

    /*
     * Current bootstrap layout identity-maps 0..4 GiB in PML4[0], using
     * 2 MiB supervisor pages. Clone every table below that slot, not just the
     * root, so future per-process edits cannot mutate the kernel's hierarchy.
     */
    uint64_t* new_pdpt = allocate_zeroed_table();
    if (!new_pdpt) {
        pmm_free_page(new_root);
        return 0;
    }
    new_root[0] = table_entry(new_pdpt,
                              VMM_PTE_PRESENT | VMM_PTE_WRITABLE | VMM_PTE_USER);

    uint64_t kernel_pml4e = kernel_root[0];
    if (kernel_pml4e & VMM_PTE_PRESENT) {
        uint64_t* kernel_pdpt = entry_table(kernel_pml4e);
        for (size_t pdpt_i = 0; pdpt_i < 512; ++pdpt_i) {
            uint64_t kernel_pdpte = kernel_pdpt[pdpt_i];
            if (!(kernel_pdpte & VMM_PTE_PRESENT)) continue;

            /* Preserve a 1 GiB leaf as a supervisor leaf, if one exists. */
            if (kernel_pdpte & VMM_PTE_LARGE) {
                new_pdpt[pdpt_i] = kernel_pdpte & ~VMM_PTE_USER;
                continue;
            }

            uint64_t* new_pd = allocate_zeroed_table();
            if (!new_pd) {
                (void)destroy_page_directory_unlocked((uint64_t)(uintptr_t)new_root);
                return 0;
            }
            new_pdpt[pdpt_i] = table_entry(
                new_pd, (kernel_pdpte & ~VMM_ADDRESS_MASK) & ~VMM_PTE_USER);

            uint64_t* kernel_pd = entry_table(kernel_pdpte);
            for (size_t pd_i = 0; pd_i < 512; ++pd_i) {
                uint64_t kernel_pde = kernel_pd[pd_i];
                if (!(kernel_pde & VMM_PTE_PRESENT)) continue;

                if (kernel_pde & VMM_PTE_LARGE) {
                    new_pd[pd_i] = kernel_pde & ~VMM_PTE_USER;
                    continue;
                }

                /* Deep-copy existing PTs too, so teardown owns all tables. */
                uint64_t* kernel_pt = entry_table(kernel_pde);
                uint64_t* new_pt = allocate_zeroed_table();
                if (!new_pt) {
                    (void)destroy_page_directory_unlocked((uint64_t)(uintptr_t)new_root);
                    return 0;
                }
                for (size_t pt_i = 0; pt_i < 512; ++pt_i)
                    new_pt[pt_i] = kernel_pt[pt_i] & ~VMM_PTE_USER;
                new_pd[pd_i] = table_entry(
                    new_pt, (kernel_pde & ~VMM_ADDRESS_MASK) & ~VMM_PTE_USER);
            }
        }
    }
    return (uint64_t)(uintptr_t)new_root;
}

void vmm_init(void) {
    uint64_t* current = (uint64_t*)(uintptr_t)(read_cr3() & VMM_ADDRESS_MASK);
    kernel_pml4_root = current;
    active_pml4_root = current;
#ifdef VMM_TESTING
    nx_supported = 1;
#else
    uint32_t max_leaf, ebx, ecx, edx;
    uint32_t leaf = 0x80000000u;
    __asm__ volatile("cpuid" : "+a"(leaf), "=b"(ebx), "=c"(ecx), "=d"(edx));
    max_leaf = leaf;
    if (max_leaf >= 0x80000001u) {
        leaf = 0x80000001u;
        __asm__ volatile("cpuid" : "+a"(leaf), "=b"(ebx), "=c"(ecx), "=d"(edx));
        if (edx & (1u << 20)) {
            uint32_t low, high;
            __asm__ volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(0xC0000080u));
            uint64_t efer = ((uint64_t)high << 32) | low;
            efer |= (1ULL << 11);
            low = (uint32_t)efer; high = (uint32_t)(efer >> 32);
            __asm__ volatile("wrmsr" : : "c"(0xC0000080u), "a"(low), "d"(high));
            nx_supported = 1;
        }
    }
#endif

    klog(LOG_KERNEL, "Paging structure validated");
    klog(LOG_KERNEL, "Virtual Memory Manager (VMM) started");
}

int vmm_map_page(uint64_t virt, uint64_t phys, uint32_t flags) {
    if (!is_page_aligned(virt) || !is_page_aligned(phys) ||
        (!is_lower_canonical(virt) && (virt >> 47) != 0x1FFFFULL) ||
        (flags & ~(VMM_FLAG_PRESENT | VMM_FLAG_WRITABLE)) != 0) {
        return -1;
    }

    spinlock_irq_flags_t lock_flags = vmm_lock_acquire();
    uint64_t* root = current_pml4_unlocked();
    uint64_t pml4_idx = (virt >> 39) & 0x1FFULL;
    uint64_t pdpt_idx = (virt >> 30) & 0x1FFULL;
    uint64_t pd_idx = (virt >> 21) & 0x1FFULL;
    uint64_t pt_idx = (virt >> 12) & 0x1FFULL;
    int new_pdpt = 0;
    int new_pd = 0;
    uint64_t* pdpt = NULL;
    uint64_t* pd = NULL;
    uint64_t* pt = NULL;

    if (!(root[pml4_idx] & VMM_PTE_PRESENT)) {
        pdpt = allocate_zeroed_table();
        if (!pdpt) goto fail;
        root[pml4_idx] = table_entry(pdpt, VMM_PTE_PRESENT | VMM_PTE_WRITABLE);
        new_pdpt = 1;
    } else {
        pdpt = entry_table(root[pml4_idx]);
    }

    if (!(pdpt[pdpt_idx] & VMM_PTE_PRESENT)) {
        pd = allocate_zeroed_table();
        if (!pd) goto fail;
        pdpt[pdpt_idx] = table_entry(pd, VMM_PTE_PRESENT | VMM_PTE_WRITABLE);
        new_pd = 1;
    } else {
        if (pdpt[pdpt_idx] & VMM_PTE_LARGE) goto fail;
        pd = entry_table(pdpt[pdpt_idx]);
    }

    if (!(pd[pd_idx] & VMM_PTE_PRESENT)) {
        pt = allocate_zeroed_table();
        if (!pt) goto fail;
        pd[pd_idx] = table_entry(pt, VMM_PTE_PRESENT | VMM_PTE_WRITABLE);
    } else {
        if (pd[pd_idx] & VMM_PTE_LARGE) goto fail;
        pt = entry_table(pd[pd_idx]);
    }

    if (pt[pt_idx] & VMM_PTE_PRESENT) goto fail;
    pt[pt_idx] = (phys & VMM_ADDRESS_MASK) |
                 VMM_PTE_PRESENT | (flags & VMM_PTE_WRITABLE);
    invlpg((void*)(uintptr_t)virt);
    vmm_lock_release(lock_flags);
    return 0;

fail:
    if (new_pd) {
        pdpt[pdpt_idx] = 0;
        pmm_free_page(pd);
    }
    if (new_pdpt) {
        root[pml4_idx] = 0;
        pmm_free_page(pdpt);
    }
    vmm_lock_release(lock_flags);
    return -1;
}

int vmm_unmap_page(uint64_t virt) {
    if (!is_page_aligned(virt)) return -1;

    spinlock_irq_flags_t lock_flags = vmm_lock_acquire();
    uint64_t* root = current_pml4_unlocked();
    uint64_t pml4_idx = (virt >> 39) & 0x1FFULL;
    uint64_t pdpt_idx = (virt >> 30) & 0x1FFULL;
    uint64_t pd_idx = (virt >> 21) & 0x1FFULL;
    uint64_t pt_idx = (virt >> 12) & 0x1FFULL;

    if (!(root[pml4_idx] & VMM_PTE_PRESENT)) goto fail;
    uint64_t* pdpt = entry_table(root[pml4_idx]);
    if (!(pdpt[pdpt_idx] & VMM_PTE_PRESENT) ||
        (pdpt[pdpt_idx] & VMM_PTE_LARGE)) goto fail;
    uint64_t* pd = entry_table(pdpt[pdpt_idx]);
    if (!(pd[pd_idx] & VMM_PTE_PRESENT) ||
        (pd[pd_idx] & VMM_PTE_LARGE)) goto fail;
    uint64_t* pt = entry_table(pd[pd_idx]);
    if (!(pt[pt_idx] & VMM_PTE_PRESENT)) goto fail;

    uint64_t pte = pt[pt_idx];
    pt[pt_idx] = 0;
    invlpg((void*)(uintptr_t)virt);
    vmm_lock_release(lock_flags);
    if (pte & VMM_PTE_USER) {
        uint64_t frame = pte & VMM_ADDRESS_MASK;
        pmm_release_user_mapping(frame);
        pmm_free_user_page((void*)(uintptr_t)frame);
    }
    return 0;

fail:
    vmm_lock_release(lock_flags);
    return -1;
}

uint64_t vmm_virt_to_phys(uint64_t virt) {
    uint64_t pml4_idx = (virt >> 39) & 0x1FFULL;
    uint64_t pdpt_idx = (virt >> 30) & 0x1FFULL;
    uint64_t pd_idx = (virt >> 21) & 0x1FFULL;
    uint64_t pt_idx = (virt >> 12) & 0x1FFULL;

    spinlock_irq_flags_t lock_flags = vmm_lock_acquire();
    uint64_t* root = current_pml4_unlocked();
    if (!(root[pml4_idx] & VMM_PTE_PRESENT)) goto unmapped;
    uint64_t* pdpt = entry_table(root[pml4_idx]);
    uint64_t pdpte = pdpt[pdpt_idx];
    if (!(pdpte & VMM_PTE_PRESENT)) goto unmapped;
    if (pdpte & VMM_PTE_LARGE) {
        uint64_t result = (pdpte & VMM_GIGA_PAGE_MASK) + (virt & 0x3FFFFFFFULL);
        vmm_lock_release(lock_flags);
        return result;
    }

    uint64_t* pd = entry_table(pdpte);
    uint64_t pde = pd[pd_idx];
    if (!(pde & VMM_PTE_PRESENT)) goto unmapped;
    if (pde & VMM_PTE_LARGE) {
        uint64_t result = (pde & VMM_LARGE_PAGE_MASK) + (virt & 0x1FFFFFULL);
        vmm_lock_release(lock_flags);
        return result;
    }

    uint64_t* pt = entry_table(pde);
    uint64_t pte = pt[pt_idx];
    if (!(pte & VMM_PTE_PRESENT)) goto unmapped;
    {
        uint64_t result = (pte & VMM_ADDRESS_MASK) + (virt & 0xFFFULL);
        vmm_lock_release(lock_flags);
        return result;
    }

unmapped:
    vmm_lock_release(lock_flags);
    return 0;
}

uint64_t vmm_get_kernel_page_directory(void) {
    spinlock_irq_flags_t lock_flags = vmm_lock_acquire();
    uint64_t result = (uint64_t)(uintptr_t)kernel_pml4_unlocked();
    vmm_lock_release(lock_flags);
    return result;
}

uint64_t vmm_create_user_page_directory(void) {
    spinlock_irq_flags_t lock_flags = vmm_lock_acquire();
    uint64_t result = clone_kernel_lower_half();
    if (result && register_address_space_unlocked(result) != 0) {
        (void)destroy_page_directory_unlocked(result);
        result = 0;
    }
    vmm_lock_release(lock_flags);
    if (!result)
        klog(LOG_ERROR, "VMM: failed to create a private page-directory hierarchy");
    return result;
}

int vmm_destroy_user_page_directory(uint64_t pml4_phys) {
    spinlock_irq_flags_t lock_flags = vmm_lock_acquire();
    int slot = address_space_slot_unlocked(pml4_phys);
    if (slot < 0) {
        vmm_lock_release(lock_flags);
        return -1;
    }
    int result = destroy_page_directory_unlocked(pml4_phys);
    if (result == 0) owned_address_spaces[slot] = 0;
    vmm_lock_release(lock_flags);
    return result;
}

void vmm_switch_page_directory(uint64_t cr3_phys) {
    if (!cr3_phys || !is_page_aligned(cr3_phys)) return;

    spinlock_irq_flags_t lock_flags = vmm_lock_acquire();
    uint64_t kernel_phys = (uint64_t)(uintptr_t)kernel_pml4_unlocked();
    if (cr3_phys != kernel_phys && address_space_slot_unlocked(cr3_phys) < 0) {
        vmm_lock_release(lock_flags);
        klog(LOG_WARNING, "VMM: refusing to switch to an unowned page directory");
        return;
    }

    uint64_t current_cr3 = read_cr3() & VMM_ADDRESS_MASK;
    if (current_cr3 != (cr3_phys & VMM_ADDRESS_MASK))
        write_cr3(cr3_phys & VMM_ADDRESS_MASK);
    active_pml4_root = (uint64_t*)(uintptr_t)(cr3_phys & VMM_ADDRESS_MASK);
    vmm_lock_release(lock_flags);
}

int vmm_map_user_page(uint64_t pml4_phys, uint64_t virt, uint64_t phys, uint32_t flags) {
    /*
     * The bootstrap uses supervisor-only 2 MiB identity pages for physical
     * memory below 4 GiB. Do not let a user mapping replace or alias any of
     * those addresses. Until a high-half kernel map exists, user virtual
     * addresses start at 4 GiB; frames still come from the current <4 GiB PMM.
     */
    if (!pml4_phys || !is_page_aligned(pml4_phys) ||
        !is_page_aligned(virt) || !is_page_aligned(phys) ||
        virt < VMM_USER_MIN_ADDRESS || !is_lower_canonical(virt) ||
        phys < VMM_RESERVED_PHYSICAL || phys >= VMM_PHYSICAL_LIMIT ||
        !pmm_is_user_page(phys) ||
        ((flags & VMM_FLAG_NO_EXECUTE) && !nx_supported) ||
        (flags & ~(VMM_FLAG_PRESENT | VMM_FLAG_WRITABLE | VMM_FLAG_USER | VMM_FLAG_NO_EXECUTE)) != 0) {
        return -1;
    }

    /* Mapping transfers exclusive ownership of this frame to the address space. */
    if (!pmm_claim_user_page(phys)) return -1;

    spinlock_irq_flags_t lock_flags = vmm_lock_acquire();
    uint64_t* kernel_root = kernel_pml4_unlocked();
    uint64_t* target_root = (uint64_t*)(uintptr_t)pml4_phys;
    if (target_root == kernel_root ||
        address_space_slot_unlocked(pml4_phys) < 0) {
        vmm_lock_release(lock_flags);
        pmm_release_user_mapping(phys);
        klog(LOG_WARNING, "VMM: refusing user mapping in an unowned page directory");
        return -1;
    }

    uint64_t pml4_idx = (virt >> 39) & 0x1FFULL;
    uint64_t pdpt_idx = (virt >> 30) & 0x1FFULL;
    uint64_t pd_idx = (virt >> 21) & 0x1FFULL;
    uint64_t pt_idx = (virt >> 12) & 0x1FFULL;
    uint64_t* pdpt = NULL;
    uint64_t* pd = NULL;
    uint64_t* pt = NULL;
    int new_pdpt = 0;
    int new_pd = 0;

    if (!(target_root[pml4_idx] & VMM_PTE_PRESENT)) {
        pdpt = allocate_zeroed_table();
        if (!pdpt) goto fail;
        target_root[pml4_idx] = table_entry(
            pdpt, VMM_PTE_PRESENT | VMM_PTE_WRITABLE | VMM_PTE_USER);
        new_pdpt = 1;
    } else {
        if (!(target_root[pml4_idx] & VMM_PTE_USER)) goto fail;
        pdpt = entry_table(target_root[pml4_idx]);
    }

    if (!(pdpt[pdpt_idx] & VMM_PTE_PRESENT)) {
        pd = allocate_zeroed_table();
        if (!pd) goto fail;
        pdpt[pdpt_idx] = table_entry(
            pd, VMM_PTE_PRESENT | VMM_PTE_WRITABLE | VMM_PTE_USER);
        new_pd = 1;
    } else {
        if (!(pdpt[pdpt_idx] & VMM_PTE_USER) ||
            (pdpt[pdpt_idx] & VMM_PTE_LARGE)) goto fail;
        pd = entry_table(pdpt[pdpt_idx]);
    }

    /*
     * Existing user-owned PTs may hold several pages in the same 2 MiB
     * region. A pre-existing huge page or supervisor PDE is never split or
     * replaced; only a missing PDE may acquire a new user PT.
     */
    if (pd[pd_idx] & VMM_PTE_PRESENT) {
        if (!(pd[pd_idx] & VMM_PTE_USER) ||
            (pd[pd_idx] & VMM_PTE_LARGE)) goto fail;
        pt = entry_table(pd[pd_idx]);
    } else {
        pt = allocate_zeroed_table();
        if (!pt) goto fail;
        pd[pd_idx] = table_entry(
            pt, VMM_PTE_PRESENT | VMM_PTE_WRITABLE | VMM_PTE_USER);
    }

    if (pt[pt_idx] & VMM_PTE_PRESENT) goto fail;
    pt[pt_idx] = (phys & VMM_ADDRESS_MASK) |
                 VMM_PTE_PRESENT | VMM_PTE_USER |
                 (flags & VMM_PTE_WRITABLE) |
                 ((flags & VMM_FLAG_NO_EXECUTE) ? VMM_PTE_NO_EXECUTE : 0);
    if ((read_cr3() & VMM_ADDRESS_MASK) == pml4_phys)
        invlpg((void*)(uintptr_t)virt);
    vmm_lock_release(lock_flags);
    return 0;

fail:
    if (new_pd) {
        pdpt[pdpt_idx] = 0;
        pmm_free_page(pd);
    }
    if (new_pdpt) {
        target_root[pml4_idx] = 0;
        pmm_free_page(pdpt);
    }
    vmm_lock_release(lock_flags);
    pmm_release_user_mapping(phys);
    return -1;
}

int vmm_unmap_user_page(uint64_t pml4_phys, uint64_t virt) {
    if (!pml4_phys || !is_page_aligned(pml4_phys) ||
        !is_page_aligned(virt) || virt < VMM_USER_MIN_ADDRESS ||
        !is_lower_canonical(virt)) return -1;
    spinlock_irq_flags_t lock_flags = vmm_lock_acquire();
    if (address_space_slot_unlocked(pml4_phys) < 0) {
        vmm_lock_release(lock_flags);
        return -1;
    }
    uint64_t* root = (uint64_t*)(uintptr_t)pml4_phys;
    uint64_t* pdpt; uint64_t* pd; uint64_t* pt;
    uint64_t pml4_idx=(virt>>39)&0x1FFULL, pdpt_idx=(virt>>30)&0x1FFULL;
    uint64_t pd_idx=(virt>>21)&0x1FFULL, pt_idx=(virt>>12)&0x1FFULL;
    if (!(root[pml4_idx]&VMM_PTE_PRESENT) || !(root[pml4_idx]&VMM_PTE_USER)) goto unmap_fail;
    pdpt=entry_table(root[pml4_idx]);
    if (!(pdpt[pdpt_idx]&VMM_PTE_PRESENT) || !(pdpt[pdpt_idx]&VMM_PTE_USER) || (pdpt[pdpt_idx]&VMM_PTE_LARGE)) goto unmap_fail;
    pd=entry_table(pdpt[pdpt_idx]);
    if (!(pd[pd_idx]&VMM_PTE_PRESENT) || !(pd[pd_idx]&VMM_PTE_USER) || (pd[pd_idx]&VMM_PTE_LARGE)) goto unmap_fail;
    pt=entry_table(pd[pd_idx]);
    if (!(pt[pt_idx]&VMM_PTE_PRESENT) || !(pt[pt_idx]&VMM_PTE_USER)) goto unmap_fail;
    {
        uint64_t frame=pt[pt_idx]&VMM_ADDRESS_MASK;
        pt[pt_idx]=0;
        if ((read_cr3() & VMM_ADDRESS_MASK) == pml4_phys)
            invlpg((void*)(uintptr_t)virt);
        vmm_lock_release(lock_flags);
        pmm_release_user_mapping(frame);
        pmm_free_user_page((void*)(uintptr_t)frame);
        return 0;
    }
unmap_fail:
    vmm_lock_release(lock_flags);
    return -1;
}

static int user_translate_unlocked(uint64_t pml4_phys, uint64_t virt,
                                      int require_write, uint64_t* phys_out) {
    if (!pml4_phys || !phys_out || virt < VMM_USER_MIN_ADDRESS ||
        !is_lower_canonical(virt)) return -1;
    uint64_t* root=(uint64_t*)(uintptr_t)pml4_phys;
    uint64_t e=root[(virt>>39)&0x1FFULL];
    if (!(e&VMM_PTE_PRESENT) || !(e&VMM_PTE_USER)) return -1;
    uint64_t* pdpt=entry_table(e);
    e=pdpt[(virt>>30)&0x1FFULL];
    if (!(e&VMM_PTE_PRESENT) || !(e&VMM_PTE_USER) || (e&VMM_PTE_LARGE)) return -1;
    uint64_t* pd=entry_table(e);
    e=pd[(virt>>21)&0x1FFULL];
    if (!(e&VMM_PTE_PRESENT) || !(e&VMM_PTE_USER) || (e&VMM_PTE_LARGE)) return -1;
    uint64_t* pt=entry_table(e);
    e=pt[(virt>>12)&0x1FFULL];
    if (!(e&VMM_PTE_PRESENT) || !(e&VMM_PTE_USER) ||
        (require_write && !(e&VMM_PTE_WRITABLE))) return -1;
    *phys_out=(e&VMM_ADDRESS_MASK)|(virt&0xFFFULL);
    return 0;
}

static int user_copy(uint64_t pml4_phys, uint64_t user_addr,
                     void* kernel_buf, size_t length, int to_user) {
    if (!kernel_buf && length) return -1;
    if (!length) return 0;
    if (user_addr < VMM_USER_MIN_ADDRESS ||
        user_addr >= VMM_USER_MAX_ADDRESS ||
        (uint64_t)length > VMM_USER_MAX_ADDRESS-user_addr) return -1;
    spinlock_irq_flags_t flags=vmm_lock_acquire();
    if (address_space_slot_unlocked(pml4_phys)<0) {
        vmm_lock_release(flags);
        return -1;
    }
    uint8_t* k=(uint8_t*)kernel_buf;
    for (size_t i=0;i<length;++i) {
        uint64_t phys;
        if (user_translate_unlocked(pml4_phys,user_addr+i,to_user,&phys)!=0) {
            vmm_lock_release(flags);
            return -1;
        }
        if (to_user) *(volatile uint8_t*)(uintptr_t)phys=k[i];
        else k[i]=*(volatile const uint8_t*)(uintptr_t)phys;
    }
    vmm_lock_release(flags);
    return 0;
}

int vmm_copy_to_user(uint64_t pml4_phys, uint64_t user_dest,
                     const void* kernel_src, size_t length) {
    return user_copy(pml4_phys,user_dest,(void*)kernel_src,length,1);
}
int vmm_copy_from_user(uint64_t pml4_phys, void* kernel_dest,
                       uint64_t user_src, size_t length) {
    return user_copy(pml4_phys,user_src,kernel_dest,length,0);
}

int vmm_ensure_identity_mapped(uint64_t phys_start, uint64_t size) {
    if (!size || phys_start >= VMM_PHYSICAL_LIMIT ||
        size > VMM_PHYSICAL_LIMIT - phys_start) return -1;
    uint64_t last = phys_start + size - 1;
    if (vmm_virt_to_phys(phys_start) != phys_start ||
        vmm_virt_to_phys(last) != last) return -1;
    return 0;
}

/* The bootstrap maps the first 4 GiB with 2 MiB pages; keep framebuffer MMIO
   cache policy changes in the kernel root and only perform them before CR3
   is switched to a per-process copy of that identity map. */
int vmm_mark_uncached(uint64_t phys_start, uint64_t size) {
    if (!size || phys_start >= VMM_PHYSICAL_LIMIT ||
        size > VMM_PHYSICAL_LIMIT - phys_start) return -1;

    spinlock_irq_flags_t lock_flags = vmm_lock_acquire();
    uint64_t* root = kernel_pml4_unlocked();
    if (current_pml4_unlocked() != root) {
        vmm_lock_release(lock_flags);
        return -1;
    }

    uint64_t first = phys_start & ~0x1FFFFFULL;
    uint64_t last = (phys_start + size - 1) & ~0x1FFFFFULL;
    __asm__ volatile("wbinvd" ::: "memory");
    for (uint64_t base = first; ; base += 0x200000ULL) {
        uint64_t pml4_idx = (base >> 39) & 0x1FFULL;
        uint64_t pdpt_idx = (base >> 30) & 0x1FFULL;
        uint64_t pd_idx = (base >> 21) & 0x1FFULL;
        if (!(root[pml4_idx] & VMM_PTE_PRESENT)) goto fail;
        uint64_t* pdpt = entry_table(root[pml4_idx]);
        if (!(pdpt[pdpt_idx] & VMM_PTE_PRESENT) ||
            (pdpt[pdpt_idx] & VMM_PTE_LARGE)) goto fail;
        uint64_t* pd = entry_table(pdpt[pdpt_idx]);
        if (!(pd[pd_idx] & VMM_PTE_PRESENT) ||
            !(pd[pd_idx] & VMM_PTE_LARGE)) goto fail;
        pd[pd_idx] |= VMM_PTE_WRITE_THROUGH | VMM_PTE_CACHE_DISABLE;
        invlpg((void*)(uintptr_t)base);
        if (base == last) break;
    }
    __asm__ volatile("wbinvd" ::: "memory");
    vmm_lock_release(lock_flags);
    return 0;

fail:
    __asm__ volatile("wbinvd" ::: "memory");
    vmm_lock_release(lock_flags);
    return -1;
}
