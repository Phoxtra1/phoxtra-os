#ifndef PHOXTRA_IDT_H
#define PHOXTRA_IDT_H

#include <stdint.h>

struct idt_entry {
    uint16_t isr_low;      // The lower 16 bits of the ISR's address
    uint16_t kernel_cs;    // The GDT segment selector (0x08 for code)
    uint8_t  ist;          // Interrupt Stack Table offset, or 0
    uint8_t  attributes;   // Type and attributes (e.g. 0x8E for 64-bit interrupt gate)
    uint16_t isr_mid;      // The middle 16 bits of the ISR's address
    uint32_t isr_high;     // The higher 32 bits of the ISR's address
    uint32_t reserved;     // Set to 0
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

void idt_set_gate(uint8_t num, uint64_t base, uint16_t sel, uint8_t flags);

// Initialize the IDT
void idt_init(void);

// Load the IDT using lidt (implemented in interrupts.asm)
void idt_load(struct idt_ptr* ptr);

#endif
