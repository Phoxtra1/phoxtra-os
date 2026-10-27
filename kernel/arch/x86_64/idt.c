#include "idt.h"

static struct idt_entry idt[256];
static struct idt_ptr idt_p;

extern void* isr_stub_table[256];

void idt_set_gate(uint8_t num, uint64_t base, uint16_t sel, uint8_t flags) {
    idt[num].isr_low = base & 0xFFFF;
    idt[num].kernel_cs = sel;
    idt[num].ist = 0;
    idt[num].attributes = flags;
    idt[num].isr_mid = (base >> 16) & 0xFFFF;
    idt[num].isr_high = (base >> 32) & 0xFFFFFFFF;
    idt[num].reserved = 0;
}

void idt_init(void) {
    idt_p.limit = (sizeof(struct idt_entry) * 256) - 1;
    idt_p.base = (uint64_t)&idt;

    // Map all 256 interrupts to their respective ISR stubs
    for (int i = 0; i < 256; i++) {
        idt_set_gate(i, (uint64_t)isr_stub_table[i], 0x08, 0x8E);
    }

    /* Permit Ring 3 to invoke the dedicated software-syscall vector. */
    idt_set_gate(0x80, (uint64_t)isr_stub_table[0x80], 0x08, 0xEE);

    // Load the IDT using the assembly loader
    idt_load(&idt_p);
}
