#include "initrd.h"
#include "../log/logger.h"
#include "../memory/heap.h"

typedef struct {
    char name[32];
    const char* content;
    uint64_t size;
} initrd_file_t;

static const char welcome_txt[] = 
    "=========================================\n"
    "   PHOXTRA OS - RAMDISK INITIALIZED      \n"
    "=========================================\n"
    "Welcome to the Phoxtra OS Virtual Ramdisk!\n"
    "System security status: ACTIVE\n"
    "User mode privilege: RING 3 ENABLED\n";

static const char system_cfg[] = 
    "# Phoxtra OS Configuration\n"
    "kernel.version=0.6.0-alpha\n"
    "security.isolation=strict\n"
    "syscall.fast_msr=enabled\n"
    "vfs.devfs=enabled\n";

static uint64_t initrd_read_cb(vfs_node_t* node, uint64_t offset, uint64_t size, uint8_t* buffer) {
    if (!node || !node->ptr) return 0;
    initrd_file_t* file = (initrd_file_t*)node->ptr;
    if (offset >= file->size) return 0;

    uint64_t bytes_to_read = size;
    if (offset + bytes_to_read > file->size) {
        bytes_to_read = file->size - offset;
    }

    for (uint64_t i = 0; i < bytes_to_read; i++) {
        buffer[i] = (uint8_t)file->content[offset + i];
    }
    return bytes_to_read;
}

void initrd_init(void) {
    vfs_node_t* initrd_dir = vfs_create_node("initrd", VFS_DIRECTORY);
    vfs_mount("initrd", initrd_dir);

    // File 1: welcome.txt
    initrd_file_t* f1 = (initrd_file_t*)kmalloc(sizeof(initrd_file_t));
    f1->content = welcome_txt;
    f1->size = sizeof(welcome_txt) - 1;

    vfs_node_t* n1 = vfs_create_node("welcome.txt", VFS_FILE);
    n1->size = f1->size;
    n1->ptr = f1;
    n1->read = initrd_read_cb;
    n1->parent = initrd_dir;
    initrd_dir->first_child = n1;

    // File 2: system.cfg
    initrd_file_t* f2 = (initrd_file_t*)kmalloc(sizeof(initrd_file_t));
    f2->content = system_cfg;
    f2->size = sizeof(system_cfg) - 1;

    vfs_node_t* n2 = vfs_create_node("system.cfg", VFS_FILE);
    n2->size = f2->size;
    n2->ptr = f2;
    n2->read = initrd_read_cb;
    n2->parent = initrd_dir;
    n1->next_sibling = n2;

    klog(LOG_KERNEL, "InitRD initial ramdisk filesystem mounted at /initrd");
}
