# Memory Architecture - Phoxtra OS v0.4.0-alpha

Phoxtra OS utilizes a 3-tier memory management system:

```
+-------------------------------------------------------+
|                 Kernel Heap (kmalloc/kfree)           |
+-------------------------------------------------------+
|             Virtual Memory Manager (VMM)              |
|        (4-Level x86_64 Paging: PML4, PDPT, PDT, PT)   |
+-------------------------------------------------------+
|            Physical Memory Manager (PMM)              |
|               (4KB Physical Page Frame Bitmap)        |
+-------------------------------------------------------+
```

## 1. Physical Memory Manager (PMM)
- **Source**: `kernel/memory/pmm.c`, `kernel/memory/pmm.h`
- **Granularity**: 4KB Page Frames.
- **Data Structure**: Bit array tracking page frame allocation state.
- **Key Functions**:
  - `pmm_init(uint64_t mem_size)`
  - `pmm_alloc_page()`
  - `pmm_free_page(void* ptr)`

## 2. Virtual Memory Manager (VMM)
- **Source**: `kernel/memory/vmm.c`, `kernel/memory/vmm.h`
- **Paging Mode**: x86_64 4-Level Paging (PML4, PDPT, PDT, PT).
- **Key Functions**:
  - `vmm_init()`
  - `vmm_map_page(uint64_t virt, uint64_t phys, uint32_t flags)`
  - `vmm_unmap_page(uint64_t virt)`

## 3. Kernel Heap Allocator
- **Source**: `kernel/memory/heap.c`, `kernel/memory/heap.h`
- **Algorithm**: Linked-list boundary tag block allocator with block splitting and free block coalescing.
- **Key Functions**:
  - `heap_init()`
  - `kmalloc(size_t size)`
  - `kfree(void* ptr)`
