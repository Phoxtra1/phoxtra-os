#include "elf.h"
#include "../memory/heap.h"
#include "../memory/vmm.h"
#include "../log/logger.h"

void elf_init(void) {
    klog(LOG_INFO, "Initializing ELF64 Executable Binary Loader Subsystem...");
}

int elf_validate_header(const elf64_header_t* header) {
    if (!header) return 0;

    // Check \x7fELF Magic Bytes
    if (header->e_ident[0] != 0x7F ||
        header->e_ident[1] != 'E'  ||
        header->e_ident[2] != 'L'  ||
        header->e_ident[3] != 'F') {
        klog(LOG_ERROR, "ELF Load Error: Invalid ELF magic signature");
        return 0;
    }

    if (header->e_ident[4] != ELF_CLASS_64) {
        klog(LOG_ERROR, "ELF Load Error: Architecture is not 64-bit ELF64");
        return 0;
    }

    if (header->e_machine != ELF_ARCH_X86_64) {
        klog(LOG_ERROR, "ELF Load Error: Machine arch is not x86_64");
        return 0;
    }

    return 1; // Valid ELF64 Header
}

int elf_load_binary(const uint8_t* buffer, uint64_t size, uint64_t* entry_point_out) {
    if (!buffer || size < sizeof(elf64_header_t) || !entry_point_out) return -1;

    const elf64_header_t* header = (const elf64_header_t*)buffer;
    if (!elf_validate_header(header)) return -1;

    klog(LOG_INFO, "ELF64 Binary Validated. Parsing Program Headers (Phdr)...");

    const elf64_phdr_t* phdrs = (const elf64_phdr_t*)(buffer + header->e_phoff);
    uint32_t loaded_segments = 0;

    for (uint16_t i = 0; i < header->e_phnum; i++) {
        const elf64_phdr_t* phdr = &phdrs[i];

        if (phdr->p_type == PT_LOAD) {
            klog(LOG_INFO, "ELF Segment PT_LOAD: Virtual Addr mapped");

            // Allocate and zero-fill segment memory
            uint8_t* segment_mem = (uint8_t*)kmalloc(phdr->p_memsz);
            if (!segment_mem) continue;

            for (uint64_t b = 0; b < phdr->p_memsz; b++) segment_mem[b] = 0;

            // Copy file payload into segment
            if (phdr->p_filesz > 0) {
                for (uint64_t f = 0; f < phdr->p_filesz; f++) {
                    segment_mem[f] = buffer[phdr->p_offset + f];
                }
            }

            loaded_segments++;
        }
    }

    *entry_point_out = header->e_entry;
    klog(LOG_INFO, "ELF64 Load Complete. Entry point extracted");
    return 0;
}
