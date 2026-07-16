.set ALIGN,    1<<0
.set MEMINFO,  1<<1
.set FLAGS,    ALIGN | MEMINFO
.set MAGIC,    0xe85250d6
.set ARCH,     0

.section .multiboot
.align 8
.long MAGIC
.long ARCH
.long 0
.long -(MAGIC + ARCH + 0)

.global _start
.extern kernel_init

.section .text

_start:
    cli

    call kernel_init

1:
    hlt
    jmp 1b
