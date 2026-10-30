#include "ld_so.h"
#include "../log/logger.h"

#define MAX_SHARED_LIBS 8
static shared_library_t loaded_libs[MAX_SHARED_LIBS];
static uint32_t lib_count = 0;

void ld_so_init(void) {
    for (uint32_t i = 0; i < MAX_SHARED_LIBS; ++i) {
        loaded_libs[i].name[0] = '\0';
        loaded_libs[i].base_address = 0;
        loaded_libs[i].symbol_count = 0;
        loaded_libs[i].loaded = 0;
    }
    lib_count = 0;
    klog(LOG_WARNING, "Dynamic linking is unavailable; only validated static ELF images are supported");
}

int ld_so_load_library(const char* lib_path) {
    if (!lib_path) return -1;
    if (lib_count >= MAX_SHARED_LIBS) return -1;
    klog(LOG_WARNING, "Shared library loading is not implemented; library was not loaded");
    return -2;
}

uint64_t ld_so_resolve_symbol(const char* lib_name, const char* symbol_name) {
    (void)lib_name;
    (void)symbol_name;
    klog(LOG_WARNING, "Dynamic symbol resolution is not implemented");
    return 0;
}
