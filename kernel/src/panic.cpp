#include "panic.hpp"
#include "console.hpp"
#include "serial.hpp"
#include <stddef.h>

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

static uint64_t read_cr2(void) {
    uint64_t value;
    asm volatile("mov %%cr2, %0" : "=r"(value));
    return value;
}

static uint64_t read_ss(void) {
    uint16_t value;
    asm volatile("mov %%ss, %0" : "=r"(value));
    return value;
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

static void print_stack_trace(uint64_t rbp, uint64_t rsp) {
    console::write("Stack trace (raw return addresses):\n");
    serial::write("Stack trace (raw return addresses):\n");

    /* Keep the walk bounded and monotonic; never chase arbitrary frame chains. */
    if (!rbp || rbp < rsp || rbp - rsp > 65536ULL || (rbp & 0x7ULL)) {
        console::write("  Frame chain unavailable.\n");
        serial::write("  Frame chain unavailable.\n");
        return;
    }

    for (unsigned depth = 0; depth < 8; ++depth) {
        if (rbp < rsp || rbp - rsp > 65536ULL || (rbp & 0x7ULL)) break;
        const uint64_t* frame = (const uint64_t*)(uintptr_t)rbp;
        uint64_t next = frame[0];
        uint64_t return_address = frame[1];
        if (!return_address) break;
        print_reg("  RETURN: ", return_address, true);
        if (next <= rbp || next - rsp > 65536ULL || next - rbp > 65536ULL) break;
        rbp = next;
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
        print_reg("RBP: ", regs->rbp, false);
        /* Same-privilege exceptions do not push RSP/SS. Reconstruct RSP from
           the normalized frame address instead of reading unrelated stack data. */
        uint64_t saved_rsp = (regs->cs & 0x3u)
            ? regs->rsp
            : (uint64_t)regs + offsetof(registers, rsp);
        uint64_t saved_ss = (regs->cs & 0x3u) ? regs->ss : read_ss();
        print_reg("RSP: ", saved_rsp, true);
        print_reg("R8:  ", regs->r8,  false); print_reg("R9:  ", regs->r9,  true);
        print_reg("R10: ", regs->r10, false); print_reg("R11: ", regs->r11, true);
        print_reg("R12: ", regs->r12, false); print_reg("R13: ", regs->r13, true);
        print_reg("R14: ", regs->r14, false); print_reg("R15: ", regs->r15, true);
        print_reg("RIP: ", regs->rip, false); print_reg("CS:  ", regs->cs,  true);
        print_reg("RFL: ", regs->rflags, false); print_reg("SS:  ", saved_ss, true);
        print_reg("VECTOR: ", regs->int_no, false); print_reg("ERR: ", regs->err_code, true);
        if (regs->int_no == 14) print_reg("CR2: ", read_cr2(), true);
        if ((regs->cs & 0x3u) == 0) print_stack_trace(regs->rbp, saved_rsp);
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
