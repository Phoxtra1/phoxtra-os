#include "gdt.hpp"
#include <stddef.h>

namespace gdt
{


static GDTEntry gdt[7];   // null, kcode, kdata, ucode, udata, tss_low, tss_high
static GDTPointer gdt_ptr;
static TSS tss;

static void set_entry(
    int index,
    uint32_t base,
    uint32_t limit,
    uint8_t access,
    uint8_t granularity)
{
    gdt[index].limit_low = limit & 0xFFFF;
    gdt[index].base_low = base & 0xFFFF;
    gdt[index].base_middle = (base >> 16) & 0xFF;
    gdt[index].access = access;
    gdt[index].granularity = ((limit >> 16) & 0x0F);
    gdt[index].granularity |= (granularity & 0xF0);
    gdt[index].base_high = (base >> 24) & 0xFF;
}

extern "C" void gdt_set_kernel_stack(uint64_t rsp0) {
    gdt::set_tss_stack(rsp0);
}

static void set_tss_descriptor(int index, uint64_t base, uint32_t limit) {
    set_entry(index, (uint32_t)base, limit, 0x89, 0x00);
    
    // High 32 bits of base address for x86_64 system descriptors
    uint64_t base_high = base >> 32;
    gdt[index + 1].limit_low = base_high & 0xFFFF;
    gdt[index + 1].base_low = (base_high >> 16) & 0xFFFF;
    gdt[index + 1].base_middle = 0;
    gdt[index + 1].access = 0;
    gdt[index + 1].granularity = 0;
    gdt[index + 1].base_high = 0;
}

void set_tss_stack(uint64_t rsp0) {
    tss.rsp0 = rsp0;
}

void init()
{
    gdt_ptr.limit = sizeof(gdt) - 1;
    gdt_ptr.base  = (uint64_t)&gdt;

    // Index 0: Null descriptor (0x00)
    set_entry(0, 0, 0, 0, 0);

    // Index 1: Kernel code — Ring 0, 64-bit — selector 0x08
    set_entry(1, 0, 0, 0x9A, 0x20);

    // Index 2: Kernel data — Ring 0, flat — selector 0x10
    set_entry(2, 0, 0, 0x92, 0x00);

    // Index 3: User code — Ring 3, 64-bit — selector 0x1B (0x18 | 3)
    set_entry(3, 0, 0, 0xFA, 0x20);

    // Index 4: User data — Ring 3, flat — selector 0x23 (0x20 | 3)
    set_entry(4, 0, 0, 0xF2, 0x00);

    // Index 5 & 6: TSS Descriptor — selector 0x28
    for (size_t i = 0; i < sizeof(TSS); i++) ((uint8_t*)&tss)[i] = 0;
    tss.iomap_base = sizeof(TSS);
    set_tss_descriptor(5, (uint64_t)&tss, sizeof(TSS) - 1);

    gdt_flush(&gdt_ptr);

    // Load Task Register (LTR) selector 0x28
    asm volatile("ltr %0" : : "r"((uint16_t)0x28));
}

}

