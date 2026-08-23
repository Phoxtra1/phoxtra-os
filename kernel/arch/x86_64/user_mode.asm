; kernel/arch/x86_64/user_mode.asm
; Milestone v0.6.0: Ring 3 User Mode Switch & Fast Syscall Entry

global switch_to_user_mode
global syscall_entry
extern syscall_handler

section .text

; void switch_to_user_mode(uint64_t entry_point, uint64_t user_rsp)
;   RDI = entry_point
;   RSI = user_rsp
switch_to_user_mode:
    cli
    mov ax, 0x23        ; Ring 3 Data Selector (0x20 | 0x3)
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push qword 0x23     ; SS: Ring 3 Data
    push rsi            ; RSP: User Stack Pointer
    push qword 0x202    ; RFLAGS: Interrupts Enabled (IF=1)
    push qword 0x1B     ; CS: Ring 3 Code (0x18 | 0x3)
    push rdi            ; RIP: User Code Entry Point
    iretq

; Fast system call entry point configured via IA32_LSTAR MSR (0xC0000082)
syscall_entry:
    ; syscall saves RIP -> RCX, RFLAGS -> R11
    push r11
    push rcx
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15
    push rdx
    push rsi
    push rdi
    push rax

    mov rdi, rsp        ; Pass pointer to CPU registers structure
    call syscall_handler

    pop rax
    pop rdi
    pop rsi
    pop rdx
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    pop rcx
    pop r11
    o64 sysret

section .note.GNU-stack noalloc noexec nowrite progbits
