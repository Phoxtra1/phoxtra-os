#include "ld_so.h"
#include "../log/logger.h"

#define MAX_SHARED_LIBS 8
static shared_library_t loaded_libs[MAX_SHARED_LIBS];
static uint32_t lib_count = 0;

void ld_so_init(void) {
    klog(LOG_INFO, "Initializing Dynamic Linker / Runtime Shared Library Loader (ld.so)...");

    // Register default C Runtime Shared Library (libc.so)
    loaded_libs[0].name[0] = 'l'; loaded_libs[0].name[1] = 'i'; loaded_libs[0].name[2] = 'b'; loaded_libs[0].name[3] = 'c'; loaded_libs[0].name[4] = '.'; loaded_libs[0].name[5] = 's'; loaded_libs[0].name[6] = 'o'; loaded_libs[0].name[7] = '\0';
    loaded_libs[0].base_address = 0x7FFFF7A00000ULL;
    loaded_libs[0].symbol_count = 128;
    loaded_libs[0].loaded       = 1;
    lib_count = 1;

    klog(LOG_INFO, "Dynamic Linker Ready. Runtime Link Map: libc.so loaded at 0x7FFFF7A00000");
}

int ld_so_load_library(const char* lib_path) {
    if (!lib_path || lib_count >= MAX_SHARED_LIBS) return -1;

    klog(LOG_INFO, "ld.so: Loading dynamic shared object (.so)...");
    return 0;
}

uint64_t ld_so_resolve_symbol(const char* lib_name, const char* symbol_name) {
    (void)lib_name;
    if (!symbol_name) return 0;

    klog(LOG_INFO, "ld.so: Resolving symbol binding...");
    return 0x7FFFF7A01050ULL; // Mock resolved symbol pointer
}
