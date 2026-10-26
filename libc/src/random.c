#include "../include/phoxtra/net.h"
#include <stddef.h>
#include <stdint.h>
int phoxtra_random_bytes(uint8_t* output,size_t length){
 if((!output&&length)||length>256u)return -22;if(!length)return 0;
 register uint64_t rax __asm__("rax")=13u;
 register uint64_t rdi __asm__("rdi")=(uint64_t)(uintptr_t)output;
 register uint64_t rsi __asm__("rsi")=(uint64_t)length;
 __asm__ volatile("int $0x80":"+a"(rax):"D"(rdi),"S"(rsi):"memory","cc");
 int64_t result=(int64_t)rax;if(result<0)return (int)result;
 return (size_t)result==length?0:-5;
}
