#include "../kernel/exec/elf.h"
#include "../kernel/memory/pmm.h"
#include "../kernel/memory/vmm.h"
#include "../kernel/log/logger.h"
#include <stdio.h>
#include <string.h>
static unsigned logs;
void klog(log_level_t level, const char* message) { (void)level; (void)message; ++logs; }
static uint8_t image[8192];
static uint8_t user_frames[16][PAGE_SIZE] __attribute__((aligned(PAGE_SIZE)));
static uint8_t frame_used[16];
static uint64_t mapped_va[32];
static uint64_t mapped_frame[32];
static uint32_t mapped_flags[32];
static unsigned mapped_count;
static int map_fail_after = -1;
void* pmm_alloc_user_page(void) {
 for (unsigned i=0;i<16;++i) if (!frame_used[i]) {
  frame_used[i]=1; memset(user_frames[i],0,PAGE_SIZE); return user_frames[i];
 }
 return 0;
}
void pmm_free_user_page(void* ptr) {
 for (unsigned i=0;i<16;++i) if (ptr==user_frames[i]) frame_used[i]=0;
}
int vmm_map_user_page(uint64_t root,uint64_t va,uint64_t phys,uint32_t flags) {
 (void)root;
 if (map_fail_after==0) return -1;
 if (map_fail_after>0) --map_fail_after;
 if (mapped_count>=32) return -1;
 mapped_va[mapped_count]=va; mapped_frame[mapped_count]=phys;
 mapped_flags[mapped_count]=flags; ++mapped_count; return 0;
}
int vmm_copy_to_user(uint64_t root,uint64_t dest,const void* src,size_t length) {
 (void)root;
 const uint8_t* input=(const uint8_t*)src;
 for (size_t n=0;n<length;++n) {
  uint64_t va=dest+n;
  unsigned i;
  for (i=0;i<mapped_count;++i)
   if (va>=mapped_va[i] && va-mapped_va[i]<PAGE_SIZE) break;
  if (i==mapped_count || !(mapped_flags[i]&VMM_FLAG_WRITABLE)) return -1;
  ((uint8_t*)(uintptr_t)mapped_frame[i])[va-mapped_va[i]]=input[n];
 }
 return 0;
}
int vmm_unmap_user_page(uint64_t root,uint64_t va) {
 (void)root;
 for (unsigned i=0;i<mapped_count;++i) if (mapped_va[i]==va && mapped_flags[i]) {
  mapped_flags[i]=0;
  for (unsigned j=0;j<16;++j) if (mapped_frame[i]==(uint64_t)(uintptr_t)user_frames[j]) frame_used[j]=0;
  return 0;
 }
 return -1;
}
static elf64_header_t* valid_image(void) {
 memset(image, 0, sizeof(image));
 elf64_header_t* h = (elf64_header_t*)image;
 h->e_ident[0]=0x7f; h->e_ident[1]='E'; h->e_ident[2]='L'; h->e_ident[3]='F';
 h->e_ident[4]=ELF_CLASS_64; h->e_ident[5]=ELF_DATA_LSB; h->e_ident[6]=ELF_VERSION_CURRENT;
 h->e_type=ELF_TYPE_EXEC; h->e_machine=ELF_ARCH_X86_64; h->e_version=ELF_VERSION_CURRENT;
 h->e_entry=0x100000000ULL; h->e_phoff=sizeof(*h); h->e_ehsize=sizeof(*h);
 h->e_phentsize=sizeof(elf64_phdr_t); h->e_phnum=1;
 elf64_phdr_t* p=(elf64_phdr_t*)(image+h->e_phoff);
 p->p_type=PT_LOAD; p->p_flags=PF_R|PF_X; p->p_offset=0x1000; p->p_vaddr=0x100000000ULL;
 p->p_filesz=4; p->p_memsz=8; p->p_align=0x1000;
 image[0x1000]=0x90; image[0x1001]=0x90; image[0x1002]=0x90; image[0x1003]=0xC3;
 return h;
}
#define CHECK(label, expr) do { if (!(expr)) { fprintf(stderr,"FAIL: %s\n",label); return 1; } printf("PASS: %s\n",label); } while (0)
int main(void) {
 uint64_t entry=0; elf64_header_t* h=valid_image();
 CHECK("valid static ELF64 image",elf_validate_image(image,sizeof(image),&entry)&&entry==0x100000000ULL);
 h=valid_image(); mapped_count=0; memset(frame_used,0,sizeof(frame_used)); map_fail_after=-1;
 uint64_t stack_top=0;
 CHECK("load ELF segments and private stack",elf_load_into_address_space(0x1000,image,sizeof(image),&entry,&stack_top)==0 && entry==0x100000000ULL && stack_top==(0x7FFFFFF00000ULL-6ULL*sizeof(uint64_t)) && mapped_count==9);
 CHECK("ELF code bytes copied to mapped frame",((uint8_t*)(uintptr_t)mapped_frame[0])[0]==0x90 && ((uint8_t*)(uintptr_t)mapped_frame[0])[3]==0xC3);
 CHECK("user stack pages are writable and non-executable",(mapped_flags[1] & VMM_FLAG_NO_EXECUTE) && (mapped_flags[1] & VMM_FLAG_WRITABLE));
 h=valid_image(); mapped_count=0; memset(frame_used,0,sizeof(frame_used)); map_fail_after=1; stack_top=123;
 CHECK("loader reports mapping failure",elf_load_into_address_space(0x1000,image,sizeof(image),&entry,&stack_top)==-1 && stack_top==0);
 CHECK("loader rolls back all partial mappings",mapped_count==1 && mapped_flags[0]==0 && frame_used[0]==0);
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
 h=valid_image(); h->e_entry=0x100500000ULL;
 CHECK("reject entry outside executable segment",!elf_validate_image(image,sizeof(image),&entry));
 h=valid_image(); ((elf64_phdr_t*)(image+h->e_phoff))->p_vaddr=0x400000ULL; h->e_entry=0x400000ULL;
 CHECK("reject low addresses overlapping kernel identity map",!elf_validate_image(image,sizeof(image),&entry));
 h=valid_image(); ((elf64_phdr_t*)(image+h->e_phoff))->p_type=PT_DYNAMIC;
 CHECK("reject unsupported dynamic image",!elf_validate_image(image,sizeof(image),&entry));
 h=valid_image(); h->e_entry=0x100000800ULL; ((elf64_phdr_t*)(image+h->e_phoff))->p_vaddr=0x100000800ULL;
 ((elf64_phdr_t*)(image+h->e_phoff))->p_offset=0x1800ULL;
 ((elf64_phdr_t*)(image+h->e_phoff))->p_filesz=4;
 ((elf64_phdr_t*)(image+h->e_phoff))->p_memsz=8;
 elf64_phdr_t* second=(elf64_phdr_t*)(image+h->e_phoff+sizeof(elf64_phdr_t));
 *second=*((elf64_phdr_t*)(image+h->e_phoff)); second->p_vaddr=0x100000900ULL;
 second->p_offset=0x1900ULL; h->e_phnum=2;
 CHECK("reject segments sharing a mapped page",!elf_validate_image(image,sizeof(image),&entry));
 h=valid_image(); ((elf64_phdr_t*)(image+h->e_phoff))->p_flags=PF_R|PF_W|PF_X;
 CHECK("reject writable-executable segments",!elf_validate_image(image,sizeof(image),&entry));
 h=valid_image(); ((elf64_phdr_t*)(image+h->e_phoff))->p_memsz=0x5000000ULL;
 CHECK("reject image exceeding per-process page budget",!elf_validate_image(image,sizeof(image),&entry));
 h=valid_image(); ((elf64_phdr_t*)(image+h->e_phoff))->p_vaddr=0x7FFFFFF00000ULL-0x1000ULL;
 ((elf64_phdr_t*)(image+h->e_phoff))->p_memsz=0x2000ULL;
 CHECK("reject ELF segment overlapping reserved user stack",!elf_validate_image(image,sizeof(image),&entry));
 h=valid_image(); ((elf64_phdr_t*)(image+h->e_phoff))->p_vaddr=UINT64_MAX-3;
 CHECK("reject overflowing virtual range",!elf_validate_image(image,sizeof(image),&entry));
 h=valid_image(); entry=123;
 CHECK("do not report execution success for an unmapped image",elf_load_binary(image,sizeof(image),&entry)==-2&&entry==0);
 printf("ELF validation tests passed (%u diagnostic calls).\n",logs); return 0;
}
