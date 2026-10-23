#include "../kernel/exec/elf.h"
#include "../kernel/log/logger.h"
#include <stdio.h>
#include <string.h>
static unsigned logs;
void klog(log_level_t level, const char* message) { (void)level; (void)message; ++logs; }
static uint8_t image[8192];
static elf64_header_t* valid_image(void) {
 memset(image, 0, sizeof(image));
 elf64_header_t* h = (elf64_header_t*)image;
 h->e_ident[0]=0x7f; h->e_ident[1]='E'; h->e_ident[2]='L'; h->e_ident[3]='F';
 h->e_ident[4]=ELF_CLASS_64; h->e_ident[5]=ELF_DATA_LSB; h->e_ident[6]=ELF_VERSION_CURRENT;
 h->e_type=ELF_TYPE_EXEC; h->e_machine=ELF_ARCH_X86_64; h->e_version=ELF_VERSION_CURRENT;
 h->e_entry=0x400000; h->e_phoff=sizeof(*h); h->e_ehsize=sizeof(*h);
 h->e_phentsize=sizeof(elf64_phdr_t); h->e_phnum=1;
 elf64_phdr_t* p=(elf64_phdr_t*)(image+h->e_phoff);
 p->p_type=PT_LOAD; p->p_flags=PF_R|PF_X; p->p_offset=0x1000; p->p_vaddr=0x400000;
 p->p_filesz=4; p->p_memsz=8; p->p_align=0x1000;
 image[0x1000]=0x90; image[0x1001]=0x90; image[0x1002]=0x90; image[0x1003]=0xC3;
 return h;
}
#define CHECK(label, expr) do { if (!(expr)) { fprintf(stderr,"FAIL: %s\n",label); return 1; } printf("PASS: %s\n",label); } while (0)
int main(void) {
 uint64_t entry=0; elf64_header_t* h=valid_image();
 CHECK("valid static ELF64 image",elf_validate_image(image,sizeof(image),&entry)&&entry==0x400000);
 h=valid_image(); h->e_phoff=UINT64_MAX-4;
 CHECK("reject overflowing program-header table",!elf_validate_image(image,sizeof(image),&entry));
 h=valid_image(); h->e_phentsize--;
 CHECK("reject wrong program-header entry size",!elf_validate_image(image,sizeof(image),&entry));
 h=valid_image(); ((elf64_phdr_t*)(image+h->e_phoff))->p_offset=sizeof(image)-2;
 CHECK("reject truncated segment payload",!elf_validate_image(image,sizeof(image),&entry));
 h=valid_image(); ((elf64_phdr_t*)(image+h->e_phoff))->p_filesz=9;
 CHECK("reject filesz larger than memsz",!elf_validate_image(image,sizeof(image),&entry));
 h=valid_image(); ((elf64_phdr_t*)(image+h->e_phoff))->p_align=3;
 CHECK("reject invalid alignment",!elf_validate_image(image,sizeof(image),&entry));
 h=valid_image(); h->e_entry=0x500000;
 CHECK("reject entry outside executable segment",!elf_validate_image(image,sizeof(image),&entry));
 h=valid_image(); ((elf64_phdr_t*)(image+h->e_phoff))->p_type=PT_DYNAMIC;
 CHECK("reject unsupported dynamic image",!elf_validate_image(image,sizeof(image),&entry));
 h=valid_image(); ((elf64_phdr_t*)(image+h->e_phoff))->p_vaddr=UINT64_MAX-3;
 CHECK("reject overflowing virtual range",!elf_validate_image(image,sizeof(image),&entry));
 h=valid_image(); entry=123;
 CHECK("do not report execution success for an unmapped image",elf_load_binary(image,sizeof(image),&entry)==-2&&entry==0);
 printf("ELF validation tests passed (%u diagnostic calls).\n",logs); return 0;
}
