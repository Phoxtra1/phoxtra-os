.section .multiboot
.align 8

header_start:
.long 0xE85250D6         # magic
.long 0                  # architecture (i386 32-bit protected mode)
.long header_end - header_start # header length
.long -(0xE85250D6 + 0 + (header_end - header_start)) # checksum

# end tag
.short 0
.short 0
.long 8
header_end:

.section .bss
.align 4096
p4_table:
    .skip 4096
p3_table:
    .skip 4096
p2_table:
    .skip 4096

.align 16
stack_bottom:
.skip 16384
stack_top:

.section .text
.code32
.global _start
.type _start, @function

.extern kernel_init

_start:
    cli
    cld

    # Save multiboot information
    mov %eax, %edi
    mov %ebx, %esi

    # Point PML4[0] to p3_table
    mov $p3_table, %eax
    or $0x003, %eax # Present | Writable
    mov %eax, p4_table
    movl $0, p4_table + 4

    # Point PDPT[0] to p2_table
    mov $p2_table, %eax
    or $0x003, %eax # Present | Writable
    mov %eax, p3_table
    movl $0, p3_table + 4

    # Set PDT entries (map 128MB = 64 entries of 2MB)
    mov $p2_table, %edx
    mov $0, %eax
    mov $64, %ecx
.map_p2:
    mov %eax, %ebx
    or $0x083, %ebx # Present | Writable | HugePage (0x80)
    mov %ebx, (%edx)
    movl $0, 4(%edx)
    add $0x200000, %eax
    add $8, %edx
    dec %ecx
    jnz .map_p2

    # Load PML4 address to CR3
    mov $p4_table, %eax
    mov %eax, %cr3

    # Enable PAE in CR4
    mov %cr4, %eax
    or $0x20, %eax # set PAE bit (bit 5)
    mov %eax, %cr4

    # Enable Long Mode in EFER MSR
    mov $0xC0000080, %ecx # EFER MSR address
    rdmsr
    or $0x100, %eax # set LME bit (bit 8)
    wrmsr

    # Enable paging
    mov %cr0, %eax
    or $0x80000001, %eax # set PG bit (bit 31) and PE bit (bit 0)
    mov %eax, %cr0

    # Load 64-bit GDT
    lgdt gdt64_pointer

    # Far return to 64-bit code segment
    push $0x08
    push $long_mode_start
    lret

.code64
long_mode_start:
    # Load 64-bit data segment registers
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss

    # Load 64-bit stack pointer
    mov $stack_top, %rsp

    # Call 64-bit C++ kernel_init
    call kernel_init

.hang:
    hlt
    jmp .hang

.section .rodata
.align 8
gdt64:
    .quad 0x0000000000000000 # null descriptor
gdt64_code:
    .quad 0x00209A0000000000 # 64-bit code descriptor
gdt64_data:
    .quad 0x0000920000000000 # 64-bit data descriptor
gdt64_end:

gdt64_pointer:
    .short gdt64_end - gdt64 - 1
    .long gdt64

.section .note.GNU-stack,"",@progbits
