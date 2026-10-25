#include "elf.h"
#include "../process/process.h"
#include "../log/logger.h"
#include "../memory/pmm.h"
#include <stdint.h>
#include <stddef.h>

#define SMOKE_BASE 0x0000000100000000ULL
#define SMOKE_IMAGE_SIZE 0x1100u

static uint8_t smoke_image[SMOKE_IMAGE_SIZE];

void user_mode_smoke_init(void) {
    for (size_t i = 0; i < sizeof(smoke_image); ++i) smoke_image[i] = 0;
    elf64_header_t* h = (elf64_header_t*)smoke_image;
    h->e_ident[0] = 0x7F; h->e_ident[1] = 'E'; h->e_ident[2] = 'L'; h->e_ident[3] = 'F';
    h->e_ident[4] = ELF_CLASS_64; h->e_ident[5] = ELF_DATA_LSB; h->e_ident[6] = ELF_VERSION_CURRENT;
    h->e_type = ELF_TYPE_EXEC; h->e_machine = ELF_ARCH_X86_64; h->e_version = ELF_VERSION_CURRENT;
    h->e_entry = SMOKE_BASE; h->e_phoff = sizeof(*h); h->e_ehsize = sizeof(*h);
    h->e_phentsize = sizeof(elf64_phdr_t); h->e_phnum = 1;

    elf64_phdr_t* p = (elf64_phdr_t*)(smoke_image + h->e_phoff);
    p->p_type = PT_LOAD; p->p_flags = PF_R | PF_X; p->p_offset = 0x1000;
    p->p_vaddr = SMOKE_BASE; p->p_filesz = 61 + sizeof("PHOXTRA_USERMODE_SMOKE_OK");
    p->p_memsz = p->p_filesz; p->p_align = PAGE_SIZE;

    uint8_t code[61] = {
        0xB8,0x05,0,0,0,             /* mov eax, SYS_GETPID */
        0xCD,0x80,                   /* int 0x80 */
        0x48,0x85,0xC0,               /* test rax, rax */
        0x74,0x2F,                   /* hang if PID is zero */
        0xB8,0x03,0,0,0,             /* mov eax, SYS_YIELD */
        0xCD,0x80,
        0xB8,0x04,0,0,0,             /* mov eax, SYS_SLEEP */
        0x48,0xC7,0xC7,0x01,0,0,0,   /* mov rdi, 1 tick */
        0xCD,0x80,
        0xB8,0x01,0,0,0,             /* mov eax, SYS_PRINT */
        0x48,0xBF,0,0,0,0,0,0,0,0,   /* mov rdi, message address */
        0xCD,0x80,
        0xB8,0x02,0,0,0,             /* mov eax, SYS_EXIT */
        0x31,0xFF,                   /* xor edi, edi */
        0xCD,0x80,
        0xEB,0xFE                    /* should never return */
    };
    uint64_t message_address = SMOKE_BASE + sizeof(code);
    for (unsigned i = 0; i < 8; ++i) code[7 + i] = (uint8_t)(message_address >> (i * 8));
    for (size_t i = 0; i < sizeof(code); ++i) smoke_image[0x1000 + i] = code[i];
    static const char message[] = "PHOXTRA_USERMODE_SMOKE_OK";
    for (size_t i = 0; i < sizeof(message); ++i)
        smoke_image[0x1000 + sizeof(code) + i] = (uint8_t)message[i];

    pcb_t* process = process_create_user_from_elf("usermode-smoke", smoke_image,
                                                  0x1000 + p->p_filesz, 1);
    if (!process)
        klog(LOG_ERROR, "USERMODE_SMOKE_FAILED: unable to load Ring 3 smoke image");
    else
        klog(LOG_KERNEL, "USERMODE_SMOKE_QUEUED: Ring 3 syscall test scheduled");
}
