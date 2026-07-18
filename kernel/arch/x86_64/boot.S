.section .multiboot
.align 8

.long 0xE85250D6              # Multiboot2 magic
.long 0                       # Architecture (i386)
.long header_end - header_start
.long -(0xE85250D6 + 0 + (header_end - header_start))

header_start:

.short 0
.short 0
.long 8

header_end:

.section .bss
.align 16

stack_bottom:
.skip 16384
stack_top:

.section .text
.global _start
.extern kernel_init

_start:

    mov $stack_top, %rsp

    call kernel_init

1:
    cli
    hlt
    jmp 1b

.section .note.GNU-stack,"",@progbits
