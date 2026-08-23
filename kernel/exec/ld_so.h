#ifndef PHOXTRA_LD_SO_H
#define PHOXTRA_LD_SO_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct shared_symbol {
    char name[64];
    uint64_t address;
    uint32_t flags;
} shared_symbol_t;

typedef struct shared_library {
    char name[64];
    uint64_t base_address;
    uint32_t symbol_count;
    uint8_t loaded;
} shared_library_t;

void ld_so_init(void);
int ld_so_load_library(const char* lib_path);
uint64_t ld_so_resolve_symbol(const char* lib_name, const char* symbol_name);

#ifdef __cplusplus
}
#endif

#endif
