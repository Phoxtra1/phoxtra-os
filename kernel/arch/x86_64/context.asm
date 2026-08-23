; kernel/arch/x86_64/context.asm
; Context switch for Phoxtra OS v0.5.0-alpha
;
; void context_switch(uint64_t* old_rsp, uint64_t new_rsp)
;   RDI = pointer to save old RSP into
;   RSI = new RSP to switch to
;
; Strategy: software-managed register save/restore via the stack.
; IRETQ-based switching keeps the CPU in Ring 0 kernel mode throughout.

global context_switch

section .text

context_switch:
    ; ── Save callee-saved registers onto current stack ──────────────────────
    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15

    ; ── Save current stack pointer into old_rsp ──────────────────────────────
    ; RDI holds the pointer to the pcb->rsp field of the outgoing process
    mov [rdi], rsp

    ; ── Load new stack pointer from next process ──────────────────────────────
    ; RSI holds the new RSP value
    mov rsp, rsi

    ; ── Restore callee-saved registers from new stack ────────────────────────
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx

    ; ── Return — jumps to the next process's instruction pointer ─────────────
    ret

section .note.GNU-stack noalloc noexec nowrite progbits
