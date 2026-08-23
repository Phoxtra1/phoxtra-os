#ifndef PHOXTRA_GDT_HPP
#define PHOXTRA_GDT_HPP

#include <stdint.h>

namespace gdt
{

struct [[gnu::packed]] GDTEntry
{
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
};

struct [[gnu::packed]] GDTPointer
{
    uint16_t limit;
    uint64_t base;
};

struct [[gnu::packed]] TSS
{
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iomap_base;
};

extern "C" void gdt_flush(GDTPointer* ptr);
extern "C" void switch_to_user_mode(uint64_t entry_point, uint64_t user_rsp);
void set_tss_stack(uint64_t rsp0);

void init();

}

#endif

