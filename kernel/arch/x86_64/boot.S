.section .multiboot
.align 8
header_start:
.long 0xE85250D6
.long 0
.long header_end - header_start
.long -(0xE85250D6 + 0 + (header_end - header_start))
# Request a 1024x768x32 framebuffer from the Multiboot2 loader.
.short 5
.short 0
.long 20
.long 1024
.long 768
.long 32
.align 8
.short 0
.short 0
.long 8
header_end:

.section .data
.align 8
multiboot2_magic_saved: .long 0
multiboot2_info_saved:  .long 0

.section .bss
.align 4096
p4_table: .skip 4096
p3_table: .skip 4096
# Four page directories identity-map 0..4 GiB with 2 MiB pages.
p2_tables: .skip 16384
.align 16
stack_bottom: .skip 16384
stack_top:

.section .text
.code32
.global _start
.type _start, @function
.extern kernel_init
_start:
    cli
    cld
    # Preserve Multiboot2 EAX magic and EBX information pointer.
    mov %eax, multiboot2_magic_saved
    mov %ebx, multiboot2_info_saved

    # PML4[0] -> PDPT.
    mov $p3_table, %eax
    or $0x003, %eax
    mov %eax, p4_table
    movl $0, p4_table + 4

    # PDPT entries 0..3 -> four page directories (0..4 GiB).
    mov $p2_tables, %eax
    mov $p3_table, %edx
    mov $4, %ecx
.map_pdpt:
    mov %eax, %ebx
    or $0x003, %ebx
    mov %ebx, (%edx)
    movl $0, 4(%edx)
    add $4096, %eax
    add $8, %edx
    loop .map_pdpt

    # Fill 2048 huge pages to identity-map the first 4 GiB.
    mov $p2_tables, %edx
    mov $0, %eax
    mov $2048, %ecx
.map_p2:
    mov %eax, %ebx
    or $0x083, %ebx
    mov %ebx, (%edx)
    movl $0, 4(%edx)
    add $0x200000, %eax
    add $8, %edx
    loop .map_p2

    mov $p4_table, %eax
    mov %eax, %cr3
    # Long-mode user code follows the x86-64 ABI and may use SSE2.
    # Enable PAE, OSFXSR and OSXMMEXCPT before any Ring 3 code can run.
    mov %cr4, %eax
    or $0x620, %eax
    mov %eax, %cr4
    mov $0xC0000080, %ecx
    rdmsr
    or $0x100, %eax
    wrmsr
    # Enable paging/protected mode and native x87 errors, keep the FPU
    # available, and clear EM/TS so x87/SSE instructions do not #UD/#NM.
    mov %cr0, %eax
    and $~0x0C, %eax
    or $0x80000023, %eax
    mov %eax, %cr0

    lgdt gdt64_pointer
    push $0x08
    push $long_mode_start
    lret

.code64
long_mode_start:
    # Start from a deterministic FPU/SSE state. The scheduler saves and
    # restores this state for every process context switch.
    fninit
    ldmxcsr mxcsr_default(%rip)
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss
    mov $stack_top, %rsp
    and $-16, %rsp
    # SysV AMD64: kernel_init(magic, info_phys).
    mov multiboot2_magic_saved(%rip), %edi
    mov multiboot2_info_saved(%rip), %esi
    call kernel_init
.hang:
    hlt
    jmp .hang

.section .rodata
.align 4
mxcsr_default: .long 0x1F80
.align 8
gdt64: .quad 0
gdt64_code: .quad 0x00209A0000000000
gdt64_data: .quad 0x0000920000000000
gdt64_end:
gdt64_pointer:
.short gdt64_end - gdt64 - 1
.long gdt64

.section .note.GNU-stack,"",@progbits
