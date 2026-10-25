#include "pmm.h"
#include "../sync/spinlock.h"
#include "../log/logger.h"

#define DEFAULT_MEM_SIZE (128ULL * 1024ULL * 1024ULL)
#define PHYSICAL_LIMIT (4ULL * 1024ULL * 1024ULL * 1024ULL)
#define MAX_PAGES (PHYSICAL_LIMIT / PAGE_SIZE)
#define BITMAP_SIZE (MAX_PAGES / 8ULL)
#define MB2_BOOT_MAGIC 0x36D76289u
#define MB2_TAG_END 0u
#define MB2_TAG_MMAP 6u
#define MB2_MMAP_AVAILABLE 1u
#define RESERVED_LOW_BYTES (4ULL * 1024ULL * 1024ULL)

typedef struct __attribute__((packed)) {
    uint32_t total_size;
    uint32_t reserved;
} mb2_info_header_t;

typedef struct __attribute__((packed)) {
    uint32_t type;
    uint32_t size;
} mb2_tag_t;

typedef struct __attribute__((packed)) {
    uint32_t type;
    uint32_t size;
    uint32_t entry_size;
    uint32_t entry_version;
} mb2_mmap_tag_t;

typedef struct __attribute__((packed)) {
    uint64_t address;
    uint64_t length;
    uint32_t type;
    uint32_t reserved;
} mb2_mmap_entry_t;

static uint8_t pmm_bitmap[BITMAP_SIZE];
static uint8_t allocated_bitmap[BITMAP_SIZE];
static uint8_t user_allocated_bitmap[BITMAP_SIZE];
static uint64_t total_memory_bytes = DEFAULT_MEM_SIZE;
static uint64_t total_pages_count = DEFAULT_MEM_SIZE / PAGE_SIZE;
static uint64_t free_pages_count;
static spinlock_t pmm_lock = SPINLOCK_INITIALIZER;
static uint32_t boot_magic;
static uint32_t boot_info_phys;

extern char __kernel_start;
extern char __kernel_end;

static inline void bitmap_set(uint64_t page) {
    pmm_bitmap[page / 8ULL] |= (uint8_t)(1u << (page % 8ULL));
}
static inline void bitmap_clear(uint64_t page) {
    pmm_bitmap[page / 8ULL] &= (uint8_t)~(1u << (page % 8ULL));
}
static inline int bitmap_test(uint64_t page) {
    return (pmm_bitmap[page / 8ULL] & (uint8_t)(1u << (page % 8ULL))) != 0;
}
static inline void allocated_set(uint64_t page) {
    allocated_bitmap[page / 8ULL] |= (uint8_t)(1u << (page % 8ULL));
}
static inline void allocated_clear(uint64_t page) {
    allocated_bitmap[page / 8ULL] &= (uint8_t)~(1u << (page % 8ULL));
}
static inline void user_allocated_set(uint64_t page) {
    user_allocated_bitmap[page / 8ULL] |= (uint8_t)(1u << (page % 8ULL));
}
static inline void user_allocated_clear(uint64_t page) {
    user_allocated_bitmap[page / 8ULL] &= (uint8_t)~(1u << (page % 8ULL));
}
static inline int user_allocated_test(uint64_t page) {
    return (user_allocated_bitmap[page / 8ULL] & (uint8_t)(1u << (page % 8ULL))) != 0;
}
static inline int allocated_test(uint64_t page) {
    return (allocated_bitmap[page / 8ULL] & (uint8_t)(1u << (page % 8ULL))) != 0;
}

void pmm_set_boot_info(uint32_t magic, uint32_t info_phys) {
    boot_magic = magic;
    boot_info_phys = info_phys;
}

static void reserve_range(uint64_t start, uint64_t end) {
    if (start >= PHYSICAL_LIMIT || end <= start) return;
    if (end > PHYSICAL_LIMIT) end = PHYSICAL_LIMIT;
    uint64_t first = start / PAGE_SIZE;
    uint64_t last = (end + PAGE_SIZE - 1ULL) / PAGE_SIZE;
    if (last > MAX_PAGES) last = MAX_PAGES;
    for (uint64_t page = first; page < last; ++page) bitmap_set(page);
}

static int load_multiboot_memory_map(void) {
    if (boot_magic != MB2_BOOT_MAGIC || !boot_info_phys) return 0;
    const mb2_info_header_t* info = (const mb2_info_header_t*)(uintptr_t)boot_info_phys;
    if (info->total_size < sizeof(*info) || info->total_size > 16u * 1024u * 1024u) return 0;

    uint64_t highest_end = 0;
    uint64_t available_bytes = 0;
    int found_map = 0;
    uint32_t offset = sizeof(*info);
    while (offset <= info->total_size - sizeof(mb2_tag_t)) {
        const mb2_tag_t* tag = (const mb2_tag_t*)((const uint8_t*)info + offset);
        if (tag->size < sizeof(*tag) || tag->size > info->total_size - offset) return 0;
        if (tag->type == MB2_TAG_END) break;
        if (tag->type == MB2_TAG_MMAP && tag->size >= sizeof(mb2_mmap_tag_t)) {
            const mb2_mmap_tag_t* mmap = (const mb2_mmap_tag_t*)tag;
            if (mmap->entry_size < sizeof(mb2_mmap_entry_t)) return 0;
            uint32_t pos = offset + sizeof(*mmap);
            uint32_t tag_end = offset + tag->size;
            while (pos <= tag_end && mmap->entry_size <= tag_end - pos) {
                const mb2_mmap_entry_t* entry = (const mb2_mmap_entry_t*)((const uint8_t*)info + pos);
                if (entry->type == MB2_MMAP_AVAILABLE && entry->length && entry->address < PHYSICAL_LIMIT) {
                    uint64_t end = entry->length > PHYSICAL_LIMIT - entry->address
                        ? PHYSICAL_LIMIT : entry->address + entry->length;
                    uint64_t first = (entry->address + PAGE_SIZE - 1ULL) / PAGE_SIZE;
                    uint64_t last = end / PAGE_SIZE;
                    if (last > MAX_PAGES) last = MAX_PAGES;
                    for (uint64_t page = first; page < last; ++page) bitmap_clear(page);
                    if (end > highest_end) highest_end = end;
                    available_bytes += end - entry->address;
                    found_map = 1;
                }
                pos += mmap->entry_size;
            }
            break;
        }
        uint32_t next = (tag->size + 7u) & ~7u;
        if (next < tag->size || next > info->total_size - offset) break;
        offset += next;
    }
    if (!found_map || !highest_end) return 0;
    total_pages_count = (highest_end + PAGE_SIZE - 1ULL) / PAGE_SIZE;
    if (total_pages_count > MAX_PAGES) total_pages_count = MAX_PAGES;
    total_memory_bytes = available_bytes;
    return 1;
}

void pmm_init(uint64_t mem_size) {
    for (uint64_t i = 0; i < BITMAP_SIZE; ++i) {
        pmm_bitmap[i] = 0xFFu; /* unavailable until firmware marks RAM usable */
        allocated_bitmap[i] = 0;
        user_allocated_bitmap[i] = 0;
    }

    total_memory_bytes = DEFAULT_MEM_SIZE;
    total_pages_count = DEFAULT_MEM_SIZE / PAGE_SIZE;
    int memory_map_loaded = load_multiboot_memory_map();

    if (!memory_map_loaded) {
        /* Conservative fallback for boot environments without a valid mmap. */
        if (mem_size > 0) {
            total_memory_bytes = mem_size < PHYSICAL_LIMIT ? mem_size : PHYSICAL_LIMIT;
            total_pages_count = (total_memory_bytes + PAGE_SIZE - 1ULL) / PAGE_SIZE;
        }
        if (total_pages_count > MAX_PAGES) total_pages_count = MAX_PAGES;
        for (uint64_t page = RESERVED_LOW_BYTES / PAGE_SIZE; page < total_pages_count; ++page)
            bitmap_clear(page);
    }

    /* Never allocate low memory, boot structures, page tables, or the kernel. */
    reserve_range(0, RESERVED_LOW_BYTES);
    /* framebuffer_init reads the Multiboot2 tags later, so keep the full info
       block reserved until all boot-time consumers have finished with it. */
    if (boot_magic == MB2_BOOT_MAGIC && boot_info_phys) {
        const mb2_info_header_t* info = (const mb2_info_header_t*)(uintptr_t)boot_info_phys;
        if (info->total_size >= sizeof(*info) && info->total_size <= 16u * 1024u * 1024u)
            reserve_range(boot_info_phys, (uint64_t)boot_info_phys + info->total_size);
    }
    uint64_t kernel_start = (uint64_t)&__kernel_start;
    uint64_t kernel_end = (uint64_t)&__kernel_end;
    if (kernel_end > kernel_start) reserve_range(kernel_start, kernel_end);

    free_pages_count = 0;
    for (uint64_t page = 0; page < total_pages_count; ++page)
        if (!bitmap_test(page)) ++free_pages_count;

    klog(LOG_KERNEL, memory_map_loaded
        ? "PMM initialized from Multiboot2 memory map; low memory and kernel reserved"
        : "PMM initialized with conservative fallback memory map");
}

void* pmm_alloc_page(void) {
    spinlock_irq_flags_t flags = spin_lock_irqsave(&pmm_lock);
    for (uint64_t page = RESERVED_LOW_BYTES / PAGE_SIZE; page < total_pages_count; ++page) {
        if (!bitmap_test(page)) {
            bitmap_set(page);
            allocated_set(page);
            if (free_pages_count) --free_pages_count;
            spin_unlock_irqrestore(&pmm_lock, flags);
            return (void*)(page * PAGE_SIZE);
        }
    }
    spin_unlock_irqrestore(&pmm_lock, flags);
    klog(LOG_ERROR, "PMM: Out of physical memory");
    return NULL;
}

void* pmm_alloc_contiguous_pages(size_t page_count) {
    const uint64_t first_usable = RESERVED_LOW_BYTES / PAGE_SIZE;
    if (!page_count || page_count > total_pages_count || total_pages_count <= first_usable)
        return NULL;
    if ((uint64_t)page_count > total_pages_count - first_usable) return NULL;

    spinlock_irq_flags_t flags = spin_lock_irqsave(&pmm_lock);
    for (uint64_t first = first_usable;
         first <= total_pages_count - (uint64_t)page_count; ++first) {
        size_t run = 0;
        while (run < page_count && !bitmap_test(first + run)) ++run;
        if (run != page_count) {
            first += run;
            continue;
        }
        for (size_t i = 0; i < page_count; ++i) {
            bitmap_set(first + i);
            allocated_set(first + i);
        }
        free_pages_count -= page_count;
        spin_unlock_irqrestore(&pmm_lock, flags);
        return (void*)(uintptr_t)(first * PAGE_SIZE);
    }
    spin_unlock_irqrestore(&pmm_lock, flags);
    klog(LOG_ERROR, "PMM: no contiguous physical run satisfies request");
    return NULL;
}

/*
 * User frames are marked separately from ordinary kernel allocations. The
 * VMM will only install a user-accessible PTE for a frame obtained through
 * this API, preventing accidental exposure of kernel heap/stack/page-table
 * pages that were allocated through pmm_alloc_page().
 */
void* pmm_alloc_user_page(void) {
    void* page_ptr = pmm_alloc_page();
    if (!page_ptr) return NULL;
    uint64_t page = (uint64_t)(uintptr_t)page_ptr / PAGE_SIZE;
    spinlock_irq_flags_t flags = spin_lock_irqsave(&pmm_lock);
    if (page >= total_pages_count || page >= MAX_PAGES ||
        !allocated_test(page)) {
        spin_unlock_irqrestore(&pmm_lock, flags);
        pmm_free_page(page_ptr);
        return NULL;
    }
    user_allocated_set(page);
    spin_unlock_irqrestore(&pmm_lock, flags);
    return page_ptr;
}

int pmm_is_user_page(uint64_t phys_addr) {
    if ((phys_addr & (PAGE_SIZE - 1ULL)) != 0) return 0;
    uint64_t page = phys_addr / PAGE_SIZE;
    if (page >= total_pages_count || page >= MAX_PAGES) return 0;
    spinlock_irq_flags_t flags = spin_lock_irqsave(&pmm_lock);
    int result = allocated_test(page) && user_allocated_test(page);
    spin_unlock_irqrestore(&pmm_lock, flags);
    return result;
}

void pmm_free_user_page(void* ptr) {
    uint64_t addr = (uint64_t)(uintptr_t)ptr;
    if (!ptr || (addr & (PAGE_SIZE - 1ULL)) != 0) return;
    uint64_t page = addr / PAGE_SIZE;
    if (page >= total_pages_count || page >= MAX_PAGES) return;

    spinlock_irq_flags_t flags = spin_lock_irqsave(&pmm_lock);
    if (!allocated_test(page) || !user_allocated_test(page)) {
        spin_unlock_irqrestore(&pmm_lock, flags);
        return;
    }
    /* Clear ownership and allocation atomically to prevent a double-free race. */
    user_allocated_clear(page);
    allocated_clear(page);
    bitmap_clear(page);
    ++free_pages_count;
    spin_unlock_irqrestore(&pmm_lock, flags);
}

void pmm_free_page(void* ptr) {
    uint64_t addr = (uint64_t)(uintptr_t)ptr;
    if (!ptr || (addr & (PAGE_SIZE - 1ULL)) != 0) return;
    uint64_t page = addr / PAGE_SIZE;
    if (page >= total_pages_count || page >= MAX_PAGES) return;

    spinlock_irq_flags_t flags = spin_lock_irqsave(&pmm_lock);
    if (!allocated_test(page)) {
        spin_unlock_irqrestore(&pmm_lock, flags);
        return;
    }
    allocated_clear(page);
    user_allocated_clear(page);
    bitmap_clear(page);
    ++free_pages_count;
    spin_unlock_irqrestore(&pmm_lock, flags);
}

uint64_t pmm_get_free_memory(void) {
    spinlock_irq_flags_t flags = spin_lock_irqsave(&pmm_lock);
    uint64_t bytes = free_pages_count * PAGE_SIZE;
    spin_unlock_irqrestore(&pmm_lock, flags);
    return bytes;
}

uint64_t pmm_get_total_memory(void) {
    return total_memory_bytes;
}
