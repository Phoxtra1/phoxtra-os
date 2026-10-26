#include <stddef.h>
#include <stdint.h>

#define PHOXTRA_HEAP_BYTES (1024u * 1024u)
#define PHOXTRA_HEAP_MAGIC 0x50484F58u

/*
 * Keep the block header a multiple of max_align_t's alignment so the returned
 * payload, and every split block, can hold any normally aligned C object.
 */
typedef struct heap_block {
    size_t size;
    struct heap_block* next;
    uint32_t magic;
    uint32_t is_free;
    max_align_t alignment_padding;
} heap_block_t;

static union {
    max_align_t alignment;
    uint8_t bytes[PHOXTRA_HEAP_BYTES];
} user_heap;

static heap_block_t* first_block;

static size_t align_size(size_t size)
{
    const size_t alignment = sizeof(max_align_t);
    if (size > SIZE_MAX - (alignment - 1u)) {
        return 0;
    }
    return (size + alignment - 1u) & ~(alignment - 1u);
}

static void initialize_heap(void)
{
    if (first_block) {
        return;
    }

    first_block = (heap_block_t*)user_heap.bytes;
    first_block->size = PHOXTRA_HEAP_BYTES - sizeof(heap_block_t);
    first_block->next = NULL;
    first_block->magic = PHOXTRA_HEAP_MAGIC;
    first_block->is_free = 1u;
}

void* malloc(size_t size)
{
    if (size == 0) {
        size = 1u;
    }

    size = align_size(size);
    if (size == 0 || size > PHOXTRA_HEAP_BYTES - sizeof(heap_block_t)) {
        return NULL;
    }

    initialize_heap();

    for (heap_block_t* block = first_block; block; block = block->next) {
        if (block->magic != PHOXTRA_HEAP_MAGIC) {
            return NULL;
        }
        if (!block->is_free || block->size < size) {
            continue;
        }

        const size_t remainder = block->size - size;
        if (remainder > sizeof(heap_block_t) + sizeof(max_align_t)) {
            heap_block_t* split =
                (heap_block_t*)((uint8_t*)(block + 1) + size);
            split->size = remainder - sizeof(heap_block_t);
            split->next = block->next;
            split->magic = PHOXTRA_HEAP_MAGIC;
            split->is_free = 1u;

            block->next = split;
            block->size = size;
        }

        block->is_free = 0u;
        return (void*)(block + 1);
    }

    return NULL;
}

void free(void* pointer)
{
    if (!pointer) {
        return;
    }

    const uintptr_t address = (uintptr_t)pointer;
    const uintptr_t begin = (uintptr_t)user_heap.bytes;
    const uintptr_t end = begin + PHOXTRA_HEAP_BYTES;
    const size_t header_size = sizeof(heap_block_t);

    if (address < begin + header_size || address >= end) {
        return;
    }
    if ((address - begin) % _Alignof(max_align_t) != 0u) {
        return;
    }

    heap_block_t* block = ((heap_block_t*)pointer) - 1;
    if ((uintptr_t)block < begin ||
        block->magic != PHOXTRA_HEAP_MAGIC ||
        block->is_free) {
        return;
    }

    block->is_free = 1u;

    for (heap_block_t* current = first_block;
         current && current->next;) {
        heap_block_t* next = current->next;
        if (current->magic != PHOXTRA_HEAP_MAGIC ||
            next->magic != PHOXTRA_HEAP_MAGIC) {
            return;
        }

        const uintptr_t current_end =
            (uintptr_t)(current + 1) + current->size;
        if (current->is_free && next->is_free &&
            current_end == (uintptr_t)next) {
            current->size += sizeof(heap_block_t) + next->size;
            current->next = next->next;
        } else {
            current = next;
        }
    }
}

void* calloc(size_t count, size_t size)
{
    if (count != 0 && size > SIZE_MAX / count) {
        return NULL;
    }

    const size_t total = count * size;
    uint8_t* memory = (uint8_t*)malloc(total);
    if (!memory) {
        return NULL;
    }

    for (size_t i = 0; i < total; ++i) {
        memory[i] = 0;
    }
    return memory;
}

void* realloc(void* pointer, size_t new_size)
{
    if (!pointer) {
        return malloc(new_size);
    }
    if (new_size == 0) {
        free(pointer);
        return NULL;
    }

    const uintptr_t address = (uintptr_t)pointer;
    const uintptr_t begin = (uintptr_t)user_heap.bytes;
    const uintptr_t end = begin + PHOXTRA_HEAP_BYTES;
    const size_t header_size = sizeof(heap_block_t);
    if (address < begin + header_size || address >= end) {
        return NULL;
    }
    if ((address - begin) % _Alignof(max_align_t) != 0u) {
        return NULL;
    }

    heap_block_t* old_block = ((heap_block_t*)pointer) - 1;
    if (old_block->magic != PHOXTRA_HEAP_MAGIC || old_block->is_free) {
        return NULL;
    }
    if (old_block->size >= new_size) {
        return pointer;
    }

    void* replacement = malloc(new_size);
    if (!replacement) {
        return NULL;
    }

    uint8_t* destination = (uint8_t*)replacement;
    const uint8_t* source = (const uint8_t*)pointer;
    for (size_t i = 0; i < old_block->size; ++i) {
        destination[i] = source[i];
    }

    free(pointer);
    return replacement;
}
