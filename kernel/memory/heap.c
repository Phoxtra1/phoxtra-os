#include "heap.h"
#include "pmm.h"
#include "../log/logger.h"

#define HEAP_ALIGNMENT 16u

typedef struct heap_block {
    size_t size;
    int is_free;
    struct heap_block* next;
} heap_block_t;

static heap_block_t* heap_start = NULL;
#define HEAP_HEADER_SIZE ((sizeof(heap_block_t) + (HEAP_ALIGNMENT - 1u)) & ~(size_t)(HEAP_ALIGNMENT - 1u))

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

void heap_init(void) {
    heap_start = NULL;
    if (!heap_add_page()) {
        klog(LOG_ERROR, "Kernel heap initialization failed: no physical page available");
        return;
    }
    klog(LOG_KERNEL, "Kernel heap initialized with an on-demand page pool");
}

void* kmalloc(size_t requested_size) {
    size_t size = align_size(requested_size);
    if (!size) return NULL;

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

void kfree(void* ptr) {
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
    if (!current) return; /* Ignore pointers not owned by this heap. */

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
