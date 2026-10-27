.section .text.start,"ax",@progbits
.global _start
.type _start,@function
.extern phoxtra_https_guest_smoke_main
_start:
    andq $-16, %rsp
    call phoxtra_https_guest_smoke_main
    movl %eax, %edi
    movl $2, %eax
    int $0x80
1:
    jmp 1b
.size _start, .-_start
.section .note.GNU-stack,"",@progbits
