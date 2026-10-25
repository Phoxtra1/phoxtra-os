#include "heap.h"
#include "pmm.h"
#include "../sync/spinlock.h"
#include "../log/logger.h"

#define HEAP_ALIGNMENT 16u

typedef struct heap_block {
    size_t size;
    int is_free;
    struct heap_block* next;
} heap_block_t;

typedef struct heap_large_allocation {
    size_t page_count;
    struct heap_large_allocation* next;
} heap_large_allocation_t;

static heap_block_t* heap_start = NULL;
static spinlock_t heap_lock = SPINLOCK_INITIALIZER;
static heap_large_allocation_t* large_allocations = NULL;
#define HEAP_HEADER_SIZE ((sizeof(heap_block_t) + (HEAP_ALIGNMENT - 1u)) & ~(size_t)(HEAP_ALIGNMENT - 1u))
#define HEAP_LARGE_HEADER_SIZE ((sizeof(heap_large_allocation_t) + (HEAP_ALIGNMENT - 1u)) & ~(size_t)(HEAP_ALIGNMENT - 1u))

static size_t align_size(size_t size) {
    if (size > (size_t)-1 - (HEAP_ALIGNMENT - 1u)) return 0;
    return (size + (HEAP_ALIGNMENT - 1u)) & ~(size_t)(HEAP_ALIGNMENT - 1u);
}

static heap_block_t* heap_add_page(void) {
    void* page = pmm_alloc_page();
    if (!page) return NULL;
    heap_block_t* block = (heap_block_t*)page;
    block->size = PAGE_SIZE - HEAP_HEADER_SIZE;
    block->is_free = 1;
    block->next = NULL;
    if (!heap_start) {
        heap_start = block;
    } else {
        heap_block_t* tail = heap_start;
        while (tail->next) tail = tail->next;
        tail->next = block;
    }
    return block;
}

static void* heap_alloc_large(size_t size) {
    if (size > (size_t)-1 - HEAP_LARGE_HEADER_SIZE - (PAGE_SIZE - 1u)) return NULL;
    size_t page_count = (size + HEAP_LARGE_HEADER_SIZE + PAGE_SIZE - 1u) / PAGE_SIZE;
    if (!page_count) return NULL;
    void* base = pmm_alloc_contiguous_pages(page_count);
    if (!base) return NULL;
    heap_large_allocation_t* allocation = (heap_large_allocation_t*)base;
    allocation->page_count = page_count;
    allocation->next = large_allocations;
    large_allocations = allocation;
    return (void*)((uint8_t*)base + HEAP_LARGE_HEADER_SIZE);
}

void heap_init(void) {
    heap_start = NULL;
    large_allocations = NULL;
    if (!heap_add_page()) {
        klog(LOG_ERROR, "Kernel heap initialization failed: no physical page available");
        return;
    }
    klog(LOG_KERNEL, "Kernel heap initialized with on-demand small blocks and contiguous large allocations");
}

static void* kmalloc_unlocked(size_t requested_size) {
    size_t size = align_size(requested_size);
    if (!size) return NULL;

    /* Keep small allocations in independent one-page pools. Larger objects
       (including process stacks) receive a contiguous physical page run. */
    if (size > PAGE_SIZE - HEAP_HEADER_SIZE)
        return heap_alloc_large(size);

    for (;;) {
        heap_block_t* current = heap_start;
        while (current) {
            if (current->is_free && current->size >= size) {
                if (current->size >= size + HEAP_HEADER_SIZE + HEAP_ALIGNMENT) {
                    heap_block_t* remainder = (heap_block_t*)((uint8_t*)current +
                        HEAP_HEADER_SIZE + size);
                    remainder->size = current->size - size - HEAP_HEADER_SIZE;
                    remainder->is_free = 1;
                    remainder->next = current->next;
                    current->size = size;
                    current->next = remainder;
                }
                current->is_free = 0;
                return (void*)((uint8_t*)current + HEAP_HEADER_SIZE);
            }
            current = current->next;
        }

        /* Physical pages are not guaranteed contiguous: add a separate block. */
        if (!heap_add_page()) {
            klog(LOG_WARNING, "kmalloc: physical memory exhausted");
            return NULL;
        }
    }
}

static void kfree_unlocked(void* ptr) {
    if (!ptr) return;
    heap_block_t* current = heap_start;
    while (current) {
        void* payload = (void*)((uint8_t*)current + HEAP_HEADER_SIZE);
        if (payload == ptr) {
            if (current->is_free) return;
            current->is_free = 1;
            break;
        }
        current = current->next;
    }

    if (!current) {
        heap_large_allocation_t* previous = NULL;
        heap_large_allocation_t* allocation = large_allocations;
        while (allocation) {
            void* payload = (void*)((uint8_t*)allocation + HEAP_LARGE_HEADER_SIZE);
            if (payload == ptr) {
                size_t page_count = allocation->page_count;
                uintptr_t base = (uintptr_t)allocation;
                heap_large_allocation_t* next = allocation->next;
                if (previous) previous->next = next;
                else large_allocations = next;
                for (size_t i = 0; i < page_count; ++i)
                    pmm_free_page((void*)(base + i * PAGE_SIZE));
                return;
            }
            previous = allocation;
            allocation = allocation->next;
        }
        return; /* Ignore foreign pointers and duplicate frees. */
    }

    /* Coalesce only when two block headers really are physically adjacent. */
    current = heap_start;
    while (current && current->next) {
        heap_block_t* next = current->next;
        uint8_t* end = (uint8_t*)current + HEAP_HEADER_SIZE + current->size;
        if (current->is_free && next->is_free && end == (uint8_t*)next) {
            current->size += HEAP_HEADER_SIZE + next->size;
            current->next = next->next;
        } else {
            current = next;
        }
    }
}

/* Heap metadata is shared by all kernel contexts. Keep the lock order one-way:
   heap lock first, then PMM lock when a new/free page is needed. */
void* kmalloc(size_t requested_size) {
    spinlock_irq_flags_t flags = spin_lock_irqsave(&heap_lock);
    void* result = kmalloc_unlocked(requested_size);
    spin_unlock_irqrestore(&heap_lock, flags);
    return result;
}

void kfree(void* ptr) {
    spinlock_irq_flags_t flags = spin_lock_irqsave(&heap_lock);
    kfree_unlocked(ptr);
    spin_unlock_irqrestore(&heap_lock, flags);
}
