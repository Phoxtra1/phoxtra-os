#include "pmm.h"
#include "../log/logger.h"

#define DEFAULT_MEM_SIZE (128 * 1024 * 1024) // 128 MB default
#define MAX_PAGES (DEFAULT_MEM_SIZE / PAGE_SIZE)
#define BITMAP_SIZE (MAX_PAGES / 8)

static uint8_t pmm_bitmap[BITMAP_SIZE];
static uint64_t total_memory_bytes = DEFAULT_MEM_SIZE;
static uint64_t total_pages_count = MAX_PAGES;
static uint64_t used_pages_count = 0;
extern char __kernel_start;
extern char __kernel_end;

static inline void bitmap_set(uint64_t page_idx) {
    pmm_bitmap[page_idx / 8] |= (1 << (page_idx % 8));
}

static inline void bitmap_clear(uint64_t page_idx) {
    pmm_bitmap[page_idx / 8] &= ~(1 << (page_idx % 8));
}

static inline int bitmap_test(uint64_t page_idx) {
    return (pmm_bitmap[page_idx / 8] & (1 << (page_idx % 8))) != 0;
}

void pmm_init(uint64_t mem_size) {
    if (mem_size > 0) {
        total_memory_bytes = mem_size;
        total_pages_count = total_memory_bytes / PAGE_SIZE;
    }

    // Zero out bitmap (all free initially)
    for (uint64_t i = 0; i < BITMAP_SIZE; i++) {
        pmm_bitmap[i] = 0;
    }

    // Mark first 4MB as reserved (Kernel code, GDT, IDT, Page tables)
    uint64_t reserved_pages = (4 * 1024 * 1024) / PAGE_SIZE;
    for (uint64_t i = 0; i < reserved_pages; i++) {
        bitmap_set(i);
        used_pages_count++;
    }

    // Reserve loaded kernel, including bootstrap page tables and BSS.
    uint64_t kernel_start = (uint64_t)&__kernel_start;
    uint64_t kernel_end = (uint64_t)&__kernel_end;
    if (kernel_end > kernel_start) {
        uint64_t first = kernel_start / PAGE_SIZE;
        uint64_t last = (kernel_end + PAGE_SIZE - 1) / PAGE_SIZE;
        for (uint64_t i = first; i < last && i < total_pages_count && i < MAX_PAGES; i++) {
            if (!bitmap_test(i)) { bitmap_set(i); used_pages_count++; }
        }
    }
    klog(LOG_KERNEL, "Physical Memory Manager started; kernel pages reserved");
}

void* pmm_alloc_page(void) {
    for (uint64_t i = 0; i < total_pages_count; i++) {
        if (!bitmap_test(i)) {
            bitmap_set(i);
            used_pages_count++;
            return (void*)(i * PAGE_SIZE);
        }
    }
    klog(LOG_ERROR, "PMM: Out of physical memory!");
    return NULL;
}

void pmm_free_page(void* ptr) {
    uint64_t addr = (uint64_t)ptr;
    uint64_t page_idx = addr / PAGE_SIZE;
    if (page_idx < total_pages_count && bitmap_test(page_idx)) {
        bitmap_clear(page_idx);
        used_pages_count--;
    }
}

uint64_t pmm_get_free_memory(void) {
    return (total_pages_count - used_pages_count) * PAGE_SIZE;
}

uint64_t pmm_get_total_memory(void) {
    return total_memory_bytes;
}
