[bits 64]
section .text

extern common_stub

; ISR macro with no error code
%macro ISR_NOERRCODE 1
global isr%1
isr%1:
    push qword 0
    push qword %1
    jmp common_stub
%endmacro

; ISR macro with error code
%macro ISR_ERRCODE 1
global isr%1
isr%1:
    push qword %1
    jmp common_stub
%endmacro

; Define all 256 ISRs
%assign i 0
%rep 256
    %if i == 8 || i == 10 || i == 11 || i == 12 || i == 13 || i == 14 || i == 17 || i == 21 || i == 29 || i == 30
        ISR_ERRCODE i
    %else
        ISR_NOERRCODE i
    %endif
    %assign i i+1
%endrep

section .rodata
global isr_stub_table
isr_stub_table:
%assign i 0
%rep 256
    dq isr%+i
    %assign i i+1
%endrep

section .note.GNU-stack noalloc noexec write progbits
