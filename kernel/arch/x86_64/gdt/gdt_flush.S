.global gdt_flush

gdt_flush:
    lgdt (%rdi)

    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss

    pushq $0x08
    lea 1f(%rip), %rax
    pushq %rax
    lretq

1:
    ret

.section .note.GNU-stack,"",@progbits
