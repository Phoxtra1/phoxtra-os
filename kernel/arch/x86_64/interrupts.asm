[bits 64]
section .text

global idt_load
idt_load:
    lidt [rdi]
    ret

extern interrupt_handler
global common_stub

common_stub:
    ; Save all registers to stack
    push rax
    push rcx
    push rdx
    push rbx
    push rbp
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    ; System V AMD64 ABI passes the first parameter in rdi.
    ; Point it to the saved registers on the stack.
    mov rdi, rsp

    ; Call the high-level dispatcher
    call interrupt_handler

    ; Restore all registers from stack
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    pop rdx
    pop rcx
    pop rax

    ; Clean up interrupt number and error code
    add rsp, 16

    iretq

section .note.GNU-stack noalloc noexec write progbits
