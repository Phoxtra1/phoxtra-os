#define _GNU_SOURCE
#include "../kernel/memory/vmm.h"
#include "../kernel/memory/pmm.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#define ARENA_SIZE (32u * 1024u * 1024u)
#define PAGE_COUNT (ARENA_SIZE / PAGE_SIZE)
#define RESERVED_TEST_PAGES ((4u * 1024u * 1024u) / PAGE_SIZE)
#define PRESENT 0x001ULL
#define WRITABLE 0x002ULL
#define USER 0x004ULL
#define LARGE 0x080ULL
#define ADDR_MASK 0x000FFFFFFFFFF000ULL

static uint8_t* arena;
static uint8_t allocated[PAGE_COUNT];
static uint8_t user_allocated[PAGE_COUNT];
static uint64_t fake_cr3;
static uint64_t kernel_cr3;
static unsigned invalidations;
static int allocation_budget = -1;

void* pmm_alloc_page(void) {
    if (allocation_budget == 0) return NULL;
    if (allocation_budget > 0) --allocation_budget;
    for (size_t i = RESERVED_TEST_PAGES; i < PAGE_COUNT; ++i) {
        if (!allocated[i]) {
            allocated[i] = 1;
            void* page = arena + i * PAGE_SIZE;
            memset(page, 0, PAGE_SIZE);
            return page;
        }
    }
    return NULL;
}

void pmm_free_page(void* ptr) {
    uintptr_t address = (uintptr_t)ptr;
    uintptr_t base = (uintptr_t)arena;
    if (!ptr || address < base || address >= base + ARENA_SIZE ||
        ((address - base) & (PAGE_SIZE - 1u))) return;
    size_t index = (address - base) / PAGE_SIZE;
    if (index >= RESERVED_TEST_PAGES) allocated[index] = 0;
}


void* pmm_alloc_user_page(void) {
    void* page = pmm_alloc_page();
    if (page) {
        size_t index = ((uintptr_t)page - (uintptr_t)arena) / PAGE_SIZE;
        if (index < PAGE_COUNT) user_allocated[index] = 1;
    }
    return page;
}

int pmm_is_user_page(uint64_t phys_addr) {
    uintptr_t address = (uintptr_t)phys_addr;
    uintptr_t base = (uintptr_t)arena;
    if (address < base || address >= base + ARENA_SIZE ||
        ((address - base) & (PAGE_SIZE - 1u))) return 0;
    size_t index = (address - base) / PAGE_SIZE;
    return index < PAGE_COUNT && allocated[index] && user_allocated[index];
}

void pmm_free_user_page(void* ptr) {
    uintptr_t address = (uintptr_t)ptr;
    uintptr_t base = (uintptr_t)arena;
    if (!ptr || address < base || address >= base + ARENA_SIZE ||
        ((address - base) & (PAGE_SIZE - 1u))) return;
    size_t index = (address - base) / PAGE_SIZE;
    if (index >= PAGE_COUNT || !user_allocated[index]) return;
    user_allocated[index] = 0;
    pmm_free_page(ptr);
}

void klog(int level, const char* message) {
    (void)level;
    (void)message;
}

uint64_t vmm_test_read_cr3(void) { return fake_cr3; }
void vmm_test_write_cr3(uint64_t value) { fake_cr3 = value; }
void vmm_test_invlpg(uint64_t address) { (void)address; ++invalidations; }

static void fail(const char* message) {
    fprintf(stderr, "FAIL: %s\n", message);
    exit(EXIT_FAILURE);
}

static size_t allocated_page_count(void) {
    size_t count = 0;
    for (size_t i = 0; i < PAGE_COUNT; ++i)
        if (allocated[i]) ++count;
    return count;
}

static uint64_t* table(uint64_t entry) {
    return (uint64_t*)(uintptr_t)(entry & ADDR_MASK);
}

static int effective_user(uint64_t root_phys, uint64_t virt) {
    uint64_t* root = (uint64_t*)(uintptr_t)root_phys;
    uint64_t pml4e = root[(virt >> 39) & 0x1ff];
    if (!(pml4e & PRESENT) || !(pml4e & USER)) return 0;
    uint64_t* pdpt = table(pml4e);
    uint64_t pdpte = pdpt[(virt >> 30) & 0x1ff];
    if (!(pdpte & PRESENT) || !(pdpte & USER)) return 0;
    uint64_t* pd = table(pdpte);
    uint64_t pde = pd[(virt >> 21) & 0x1ff];
    if (!(pde & PRESENT) || !(pde & USER) || (pde & LARGE)) return 0;
    uint64_t* pt = table(pde);
    uint64_t pte = pt[(virt >> 12) & 0x1ff];
    return (pte & (PRESENT | USER)) == (PRESENT | USER);
}

int main(void) {
    void* mapping = mmap(NULL, ARENA_SIZE, PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
    if (mapping == MAP_FAILED) fail("could not allocate low physical-memory test arena");
    arena = (uint8_t*)mapping;
    if ((uintptr_t)arena >= 0x100000000ULL ||
        (uintptr_t)arena + ARENA_SIZE >= 0x100000000ULL)
        fail("test arena is outside the PMM's 32-bit physical window");

    /* Reproduce the bootstrap's supervisor-only 0..4 GiB 2 MiB identity map. */
    uint64_t* kernel_root = (uint64_t*)(void*)arena;
    uint64_t* kernel_pdpt = (uint64_t*)(void*)(arena + PAGE_SIZE);
    kernel_root[0] = ((uint64_t)(uintptr_t)kernel_pdpt & ADDR_MASK) |
                     PRESENT | WRITABLE;
    for (size_t pdpt_i = 0; pdpt_i < 4; ++pdpt_i) {
        uint64_t* pd = (uint64_t*)(void*)(arena + (2 + pdpt_i) * PAGE_SIZE);
        kernel_pdpt[pdpt_i] = ((uint64_t)(uintptr_t)pd & ADDR_MASK) |
                              PRESENT | WRITABLE;
        for (size_t pd_i = 0; pd_i < 512; ++pd_i) {
            uint64_t physical = ((pdpt_i * 512 + pd_i) * 0x200000ULL);
            pd[pd_i] = physical | PRESENT | WRITABLE | LARGE;
        }
    }

    kernel_cr3 = (uint64_t)(uintptr_t)kernel_root;
    fake_cr3 = kernel_cr3;
    vmm_init();
    if (vmm_get_kernel_page_directory() != kernel_cr3)
        fail("kernel root was not recorded");
    if (vmm_virt_to_phys(0x12345000ULL) != 0x12345000ULL)
        fail("kernel huge-page identity translation failed");

    size_t before_failed_clone = allocated_page_count();
    allocation_budget = 3;
    uint64_t failed_root = vmm_create_user_page_directory();
    allocation_budget = -1;
    if (failed_root != 0)
        fail("address-space creation unexpectedly succeeded after injected allocation failure");
    if (allocated_page_count() != before_failed_clone)
        fail("failed address-space creation leaked page-table pages");

    uint64_t user_root = vmm_create_user_page_directory();
    if (!user_root || user_root == fake_cr3)
        fail("private address-space creation failed");
    if (vmm_destroy_user_page_directory(user_root) != 0)
        fail("inactive empty address space could not be destroyed");

    user_root = vmm_create_user_page_directory();
    if (!user_root) fail("second private address-space creation failed");
    uint64_t kernel_frame = (uint64_t)(uintptr_t)pmm_alloc_page();
    uint64_t frame1 = (uint64_t)(uintptr_t)pmm_alloc_user_page();
    uint64_t frame2 = (uint64_t)(uintptr_t)pmm_alloc_user_page();
    if (!frame1 || !frame2) fail("test physical frame allocation failed");

    const uint64_t user_va1 = 0x0000000100001000ULL;
    const uint64_t user_va2 = 0x0000000100002000ULL;
    size_t before_failed_map = allocated_page_count();
    allocation_budget = 1;
    int failed_map = vmm_map_user_page(user_root, user_va1, frame1, VMM_FLAG_WRITABLE);
    allocation_budget = -1;
    if (failed_map == 0)
        fail("user mapping unexpectedly succeeded after injected allocation failure");
    if (allocated_page_count() != before_failed_map)
        fail("failed user mapping leaked page-table pages");
    if (vmm_map_user_page(user_root, user_va1, frame1, VMM_FLAG_WRITABLE) != 0)
        fail("first user mapping failed");
    if (vmm_map_user_page(user_root, user_va2, frame2, VMM_FLAG_WRITABLE) != 0)
        fail("second page in same 2 MiB user region failed");
    if (vmm_map_user_page(user_root, user_va1, frame2, VMM_FLAG_WRITABLE) == 0)
        fail("mapping collision unexpectedly replaced an existing user page");
    if (vmm_map_user_page(user_root, user_va1 + 0x3000ULL, kernel_frame, VMM_FLAG_WRITABLE) == 0)
        fail("ordinary kernel allocation was exposed through a user PTE");
    if (vmm_map_user_page(user_root, 0x2000, frame2, VMM_FLAG_WRITABLE) == 0)
        fail("user mapping was allowed in the bootstrap identity region");
    if (vmm_map_user_page(user_root, user_va1, 0x200000ULL, VMM_FLAG_WRITABLE) == 0)
        fail("user mapping was allowed to alias reserved low physical memory");
    if (vmm_map_user_page(kernel_cr3, user_va1, frame2, VMM_FLAG_WRITABLE) == 0)
        fail("mapping into kernel root was allowed");

    uint64_t* cloned_root = (uint64_t*)(uintptr_t)user_root;
    uint64_t* cloned_pdpt = table(cloned_root[0]);
    uint64_t kernel_pdpte = cloned_pdpt[0];
    if (!(kernel_pdpte & PRESENT) || (kernel_pdpte & USER))
        fail("cloned kernel identity PDPT entry lost supervisor-only protection");
    uint64_t* cloned_pd = table(kernel_pdpte);
    uint64_t kernel_pde = cloned_pd[(0x12345000ULL >> 21) & 0x1ff];
    if (!(kernel_pde & PRESENT) || !(kernel_pde & LARGE) || (kernel_pde & USER))
        fail("cloned kernel huge-page mapping lost supervisor-only protection");
    if (effective_user(user_root, 0x12345000ULL))
        fail("kernel identity mapping is user-accessible");
    if (!effective_user(user_root, user_va1) || !effective_user(user_root, user_va2))
        fail("user mapping lacks user permission at every paging level");

    vmm_switch_page_directory(user_root);
    if (vmm_virt_to_phys(user_va1) != frame1 ||
        vmm_virt_to_phys(user_va2) != frame2)
        fail("active address-space translations failed");
    if (vmm_virt_to_phys(0x12345000ULL) != 0x12345000ULL)
        fail("private address space lost kernel identity translation");
    if (vmm_destroy_user_page_directory(user_root) == 0)
        fail("active address space was incorrectly destroyed");

    vmm_switch_page_directory(kernel_cr3);
    if (vmm_virt_to_phys(user_va1) != 0)
        fail("user mapping leaked into the kernel address space");
    if (vmm_destroy_user_page_directory(user_root) != 0)
        fail("inactive private address space teardown failed");
    if (vmm_destroy_user_page_directory(user_root) == 0)
        fail("double destruction was not rejected");
    if (invalidations == 0) fail("expected TLB invalidations were not requested");

    pmm_free_page((void*)(uintptr_t)kernel_frame);
    pmm_free_user_page((void*)(uintptr_t)frame1);
    pmm_free_user_page((void*)(uintptr_t)frame2);
    munmap(mapping, ARENA_SIZE);
    puts("PASS: private address spaces preserve supervisor mappings, isolate user pages, reject collisions, and teardown safely");
    return EXIT_SUCCESS;
}
