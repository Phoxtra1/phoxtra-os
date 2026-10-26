#include <stddef.h>
#include <stdint.h>
#define HEAP_BYTES (1024u*1024u)
#define HEAP_MAGIC 0x50484F58u
typedef struct heap_block{size_t size;struct heap_block* next;uint32_t magic;uint32_t is_free;}heap_block_t;
static union{max_align_t alignment;uint8_t bytes[HEAP_BYTES];}arena;
static heap_block_t* head;
static size_t align_up(size_t n){const size_t a=sizeof(max_align_t);if(n>SIZE_MAX-(a-1u))return 0;return (n+a-1u)&~(a-1u);}
static void init_heap(void){if(head)return;head=(heap_block_t*)arena.bytes;head->size=HEAP_BYTES-sizeof(heap_block_t);head->next=0;head->magic=HEAP_MAGIC;head->is_free=1u;}
void* malloc(size_t n){
 if(!n)n=1u;n=align_up(n);if(!n||n>HEAP_BYTES-sizeof(heap_block_t))return 0;init_heap();
 for(heap_block_t* b=head;b;b=b->next){
  if(b->magic!=HEAP_MAGIC)return 0;if(!b->is_free||b->size<n)continue;size_t rest=b->size-n;
  if(rest>sizeof(heap_block_t)+sizeof(max_align_t)){heap_block_t* split=(heap_block_t*)((uint8_t*)(b+1)+n);split->size=rest-sizeof(heap_block_t);split->next=b->next;split->magic=HEAP_MAGIC;split->is_free=1u;b->next=split;b->size=n;}
  b->is_free=0u;return (void*)(b+1);
 }
 return 0;
}
void free(void* p){
 if(!p)return;uintptr_t a=(uintptr_t)p,begin=(uintptr_t)arena.bytes,end=begin+HEAP_BYTES;if(a<begin+sizeof(heap_block_t)||a>=end)return;
 heap_block_t* b=((heap_block_t*)p)-1;if((uintptr_t)b<begin||b->magic!=HEAP_MAGIC||b->is_free)return;b->is_free=1u;
 for(heap_block_t* c=head;c&&c->next;){heap_block_t* n=c->next;if(c->magic!=HEAP_MAGIC||n->magic!=HEAP_MAGIC)return;
  if(c->is_free&&n->is_free&&(uintptr_t)(c+1)+c->size==(uintptr_t)n){c->size+=sizeof(heap_block_t)+n->size;c->next=n->next;}else c=n;
 }
}
void* calloc(size_t count,size_t size){if(count&&size>SIZE_MAX/count)return 0;size_t n=count*size;uint8_t* p=(uint8_t*)malloc(n);if(!p)return 0;for(size_t i=0;i<n;++i)p[i]=0;return p;}
void* realloc(void* p,size_t n){
 if(!p)return malloc(n);if(!n){free(p);return 0;}uintptr_t a=(uintptr_t)p,begin=(uintptr_t)arena.bytes,end=begin+HEAP_BYTES;if(a<begin+sizeof(heap_block_t)||a>=end)return 0;
 heap_block_t* b=((heap_block_t*)p)-1;if(b->magic!=HEAP_MAGIC||b->is_free)return 0;if(b->size>=n)return p;
 void* q=malloc(n);if(!q)return 0;uint8_t* d=(uint8_t*)q;const uint8_t* s=(const uint8_t*)p;for(size_t i=0;i<b->size;++i)d[i]=s[i];free(p);return q;
}
