#include "random.h"
#include <stddef.h>
#include <stdint.h>
#define RDRAND_RETRIES 10u
#define RANDOM_MAX_BYTES 256u
static int cpu_supports_rdrand(void) {
    uint32_t eax=1u,ebx=0u,ecx=0u,edx=0u;
    __asm__ volatile("cpuid":"+a"(eax),"=b"(ebx),"=c"(ecx),"=d"(edx)::"cc");
    (void)ebx;(void)edx;
    return (ecx&(1u<<30))!=0;
}
static int rdrand_word(uint64_t* value) {
    if(!value)return -1;
    for(uint32_t attempt=0;attempt<RDRAND_RETRIES;++attempt) {
        uint8_t ready=0;uint64_t candidate=0;
        __asm__ volatile("rdrand %0; setc %1":"=r"(candidate),"=qm"(ready)::"cc");
        if(ready){*value=candidate;return 0;}
    }
    return -1;
}
int phoxtra_hw_random(uint8_t* output,size_t length) {
    if((!output&&length)||length>RANDOM_MAX_BYTES)return -1;
    if(!length)return 0;
    if(!cpu_supports_rdrand())return -1;
    size_t offset=0;
    while(offset<length){
        uint64_t word=0;
        if(rdrand_word(&word)!=0){for(size_t i=0;i<length;++i)output[i]=0;return -1;}
        size_t amount=length-offset;if(amount>sizeof(word))amount=sizeof(word);
        const uint8_t* bytes=(const uint8_t*)&word;
        for(size_t i=0;i<amount;++i)output[offset+i]=bytes[i];
        offset+=amount;word=0;
    }
    return 0;
}
