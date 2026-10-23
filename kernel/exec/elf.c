#include "elf.h"
#include "../log/logger.h"
#define ELF_USER_ADDRESS_LIMIT 0x0000800000000000ULL
static int range_fits(uint64_t offset, uint64_t length, uint64_t total) {
 return offset <= total && length <= total - offset;
}
static int is_power_of_two(uint64_t value) {
 return value && (value & (value - 1ULL)) == 0;
}
void elf_init(void) {
 klog(LOG_INFO, "ELF64 validation initialized; executable process loading remains unavailable");
}
int elf_validate_header(const elf64_header_t* h) {
 if (!h) return 0;
 if (h->e_ident[0] != 0x7F || h->e_ident[1] != 'E' ||
     h->e_ident[2] != 'L' || h->e_ident[3] != 'F') return 0;
 if (h->e_ident[4] != ELF_CLASS_64 || h->e_ident[5] != ELF_DATA_LSB ||
     h->e_ident[6] != ELF_VERSION_CURRENT) return 0;
 if (h->e_type != ELF_TYPE_EXEC || h->e_machine != ELF_ARCH_X86_64 ||
     h->e_version != ELF_VERSION_CURRENT) return 0;
 if (h->e_ehsize != sizeof(elf64_header_t) ||
     h->e_phentsize != sizeof(elf64_phdr_t) || h->e_phnum == 0) return 0;
 return 1;
}
int elf_validate_image(const uint8_t* buffer, uint64_t size, uint64_t* entry_out) {
 if (entry_out) *entry_out = 0;
 if (!buffer || size < sizeof(elf64_header_t) || !entry_out) return 0;
 const elf64_header_t* h = (const elf64_header_t*)buffer;
 if (!elf_validate_header(h)) {
  klog(LOG_WARNING, "ELF validation rejected unsupported or malformed header"); return 0;
 }
 uint64_t table_bytes = (uint64_t)h->e_phnum * (uint64_t)h->e_phentsize;
 if (!range_fits(h->e_phoff, table_bytes, size)) {
  klog(LOG_WARNING, "ELF validation rejected out-of-bounds program-header table"); return 0;
 }
 const elf64_phdr_t* ph = (const elf64_phdr_t*)(buffer + h->e_phoff);
 uint32_t loads = 0; int entry_executable = 0;
 for (uint16_t i = 0; i < h->e_phnum; ++i) {
  const elf64_phdr_t* p = &ph[i];
  if (p->p_type == PT_DYNAMIC || p->p_type == PT_INTERP) {
   klog(LOG_WARNING, "ELF validation rejects dynamic/interpreted images; only static ET_EXEC is supported"); return 0;
  }
  if (p->p_type != PT_LOAD) continue;
  if (!p->p_memsz || p->p_filesz > p->p_memsz ||
      !range_fits(p->p_offset, p->p_filesz, size)) {
   klog(LOG_WARNING, "ELF validation rejected invalid PT_LOAD file/memory range"); return 0;
  }
  if (p->p_vaddr >= ELF_USER_ADDRESS_LIMIT ||
      p->p_memsz > ELF_USER_ADDRESS_LIMIT - p->p_vaddr) {
   klog(LOG_WARNING, "ELF validation rejected non-user or overflowing virtual range"); return 0;
  }
  if (p->p_align > 1 && (!is_power_of_two(p->p_align) ||
      (p->p_offset & (p->p_align - 1ULL)) != (p->p_vaddr & (p->p_align - 1ULL)))) {
   klog(LOG_WARNING, "ELF validation rejected invalid PT_LOAD alignment"); return 0;
  }
  uint64_t end = p->p_vaddr + p->p_memsz;
  for (uint16_t j = 0; j < i; ++j) {
   const elf64_phdr_t* q = &ph[j];
   if (q->p_type != PT_LOAD || !q->p_memsz ||
       q->p_vaddr >= ELF_USER_ADDRESS_LIMIT ||
       q->p_memsz > ELF_USER_ADDRESS_LIMIT - q->p_vaddr) continue;
   uint64_t qend = q->p_vaddr + q->p_memsz;
   if (p->p_vaddr < qend && q->p_vaddr < end) {
    klog(LOG_WARNING, "ELF validation rejected overlapping PT_LOAD virtual ranges"); return 0;
   }
  }
  if ((p->p_flags & PF_X) && h->e_entry >= p->p_vaddr && h->e_entry < end)
   entry_executable = 1;
  ++loads;
 }
 if (!loads || !entry_executable) {
  klog(LOG_WARNING, "ELF validation rejected image without loadable code at entry point"); return 0;
 }
 *entry_out = h->e_entry;
 return 1;
}
int elf_load_binary(const uint8_t* buffer, uint64_t size, uint64_t* entry_out) {
 uint64_t validated_entry = 0;
 if (entry_out) *entry_out = 0;
 if (!elf_validate_image(buffer, size, &validated_entry)) return -1;
 /* Validation is not execution. No process address-space builder, segment permission
    mapping, user entry/exit path, or loader rollback exists yet. Never claim success. */
 klog(LOG_WARNING, "ELF image validated but execution is disabled until process address-space loading is implemented");
 return -2;
}
