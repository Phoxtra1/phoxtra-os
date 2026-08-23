#include "panic.hpp"
#include "console.hpp"
#include "serial.hpp"

struct registers {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rdi, rsi, rbp, rbx, rdx, rcx, rax;
    uint64_t int_no, err_code;
    uint64_t rip, cs, rflags, rsp, ss;
};

static void hex_to_str(uint64_t val, char* buf) {
    const char* chars = "0123456789ABCDEF";
    for (int i = 15; i >= 0; i--) {
        buf[i] = chars[val & 0xF];
        val >>= 4;
    }
    buf[16] = '\0';
}

static void print_reg(const char* name, uint64_t val, bool newline) {
    char buf[17];
    hex_to_str(val, buf);
    console::write(name);
    console::write("0x");
    console::write(buf);
    
    serial::write(name);
    serial::write("0x");
    serial::write(buf);

    if (newline) {
        console::write("\n");
        serial::write("\n");
    } else {
        console::write("  ");
        serial::write("  ");
    }
}

extern "C" void kernel_panic(const char* exception_name, struct registers* regs) {
    // Disable interrupts immediately
    asm volatile("cli");

    console::clear();
    serial::write("\n\n=== KERNEL PANIC ===\n");

    console::write("========================\n");
    console::write(" PHOXTRA KERNEL PANIC\n");
    console::write("========================\n\n");

    console::write("Exception:\n");
    console::write(exception_name);
    console::write("\n\n");

    serial::write("Exception: ");
    serial::write(exception_name);
    serial::write("\n\n");

    console::write("CPU:\n");
    console::write("x86_64\n\n");
    
    serial::write("CPU: x86_64\n\n");

    console::write("Registers:\n");
    serial::write("Registers:\n");

    if (regs) {
        print_reg("RAX: ", regs->rax, false); print_reg("RBX: ", regs->rbx, true);
        print_reg("RCX: ", regs->rcx, false); print_reg("RDX: ", regs->rdx, true);
        print_reg("RSI: ", regs->rsi, false); print_reg("RDI: ", regs->rdi, true);
        print_reg("RBP: ", regs->rbp, false); print_reg("RSP: ", regs->rsp, true);
        print_reg("R8:  ", regs->r8,  false); print_reg("R9:  ", regs->r9,  true);
        print_reg("R10: ", regs->r10, false); print_reg("R11: ", regs->r11, true);
        print_reg("R12: ", regs->r12, false); print_reg("R13: ", regs->r13, true);
        print_reg("R14: ", regs->r14, false); print_reg("R15: ", regs->r15, true);
        print_reg("RIP: ", regs->rip, false); print_reg("CS:  ", regs->cs,  true);
        print_reg("RFL: ", regs->rflags, false); print_reg("SS:  ", regs->ss, true);
        print_reg("ERR: ", regs->err_code, true);
    } else {
        console::write("No register dump available.\n");
        serial::write("No register dump available.\n");
    }

    console::write("\nSystem halted.\n");
    serial::write("\nSystem halted.\n");

    while (1) {
        asm volatile("hlt");
    }
}
