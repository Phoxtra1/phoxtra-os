#include "heap.h"
#include "pmm.h"
#include "../log/logger.h"

#define HEAP_INITIAL_PAGES 4096 // 16MB heap
#define HEAP_ALIGNMENT 8

typedef struct heap_block {
    size_t size;
    int is_free;
    struct heap_block* next;
} heap_block_t;

static heap_block_t* heap_start = NULL;

void heap_init(void) {
    // Allocate contiguous pages for kernel heap pool
    void* first_page = pmm_alloc_page();
    for (int i = 1; i < HEAP_INITIAL_PAGES; i++) {
        pmm_alloc_page(); // reserve block pool
    }

    if (!first_page) {
        klog(LOG_ERROR, "Heap initialization failed: PMM page allocation failed");
        return;
    }

    heap_start = (heap_block_t*)first_page;
    heap_start->size = (HEAP_INITIAL_PAGES * PAGE_SIZE) - sizeof(heap_block_t);
    heap_start->is_free = 1;
    heap_start->next = NULL;

    klog(LOG_KERNEL, "Kernel Heap started");
}

void* kmalloc(size_t size) {
    if (size == 0) return NULL;

    // Align size
    size = (size + (HEAP_ALIGNMENT - 1)) & ~(HEAP_ALIGNMENT - 1);

    heap_block_t* current = heap_start;
    while (current) {
        if (current->is_free && current->size >= size) {
            // Split block if excess size is large enough
            if (current->size >= size + sizeof(heap_block_t) + HEAP_ALIGNMENT) {
                heap_block_t* new_block = (heap_block_t*)((uint8_t*)current + sizeof(heap_block_t) + size);
                new_block->size = current->size - size - sizeof(heap_block_t);
                new_block->is_free = 1;
                new_block->next = current->next;

                current->size = size;
                current->next = new_block;
            }

            current->is_free = 0;
            return (void*)((uint8_t*)current + sizeof(heap_block_t));
        }
        current = current->next;
    }

    klog(LOG_WARNING, "kmalloc: Out of memory block!");
    return NULL;
}

void kfree(void* ptr) {
    if (!ptr) return;

    heap_block_t* block = (heap_block_t*)((uint8_t*)ptr - sizeof(heap_block_t));
    block->is_free = 1;

    // Coalesce free blocks
    heap_block_t* current = heap_start;
    while (current && current->next) {
        if (current->is_free && current->next->is_free) {
            current->size += sizeof(heap_block_t) + current->next->size;
            current->next = current->next->next;
        } else {
            current = current->next;
        }
    }
}
