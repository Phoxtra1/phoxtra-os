#include "initrd.h"
#include "../log/logger.h"
#include "../memory/heap.h"
typedef struct { char name[32]; const char* content; uint64_t size; } initrd_file_t;
static const char welcome_txt[] =
    "=========================================\n"
    "   PHOXTRA OS - RAMDISK INITIALIZED      \n"
    "=========================================\n"
    "Welcome to the Phoxtra OS Virtual Ramdisk!\n"
    "Prototype security status: NOT VALIDATED\n"
    "User-mode process execution: NOT IMPLEMENTED\n";
static const char system_cfg[] =
    "# Phoxtra OS Configuration\n"
    "kernel.version=experimental-x86_64-prototype\n"
    "security.isolation=not-validated\n"
    "syscall.fast_msr=entry-configured-user-runtime-unavailable\n"
    "vfs.devfs=enabled\n";
static uint64_t initrd_read_cb(vfs_node_t* node, uint64_t offset, uint64_t size, uint8_t* buffer) {
    if (!node || !node->ptr || !buffer) return 0;
    initrd_file_t* file = (initrd_file_t*)node->ptr;
    if (offset >= file->size) return 0;
    uint64_t bytes = size;
    if (bytes > file->size - offset) bytes = file->size - offset;
    for (uint64_t i = 0; i < bytes; ++i) buffer[i] = (uint8_t)file->content[offset + i];
    return bytes;
}
void initrd_init(void) {
    vfs_node_t* dir = vfs_create_node("initrd", VFS_DIRECTORY);
    if (!dir || vfs_mount("initrd", dir) != 0) {
        klog(LOG_ERROR, "InitRD mount failed; in-memory welcome files are unavailable");
        return;
    }
    initrd_file_t* f1 = (initrd_file_t*)kmalloc(sizeof(initrd_file_t));
    initrd_file_t* f2 = (initrd_file_t*)kmalloc(sizeof(initrd_file_t));
    vfs_node_t* n1 = vfs_create_node("welcome.txt", VFS_FILE);
    vfs_node_t* n2 = vfs_create_node("system.cfg", VFS_FILE);
    if (!f1 || !f2 || !n1 || !n2) {
        klog(LOG_ERROR, "InitRD file allocation failed");
        return;
    }
    f1->content = welcome_txt; f1->size = sizeof(welcome_txt) - 1;
    f2->content = system_cfg; f2->size = sizeof(system_cfg) - 1;
    n1->size = f1->size; n1->ptr = f1; n1->read = initrd_read_cb; n1->parent = dir;
    n2->size = f2->size; n2->ptr = f2; n2->read = initrd_read_cb; n2->parent = dir;
    dir->first_child = n1; n1->next_sibling = n2;
    klog(LOG_KERNEL, "InitRD initial ramdisk filesystem mounted at /initrd");
}
