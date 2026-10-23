#include "fat32.h"
#include "../memory/heap.h"
#include "../log/logger.h"

static void kstrncpy(char* dst, const char* src, int maxlen) {
    int i;
    for (i = 0; i < maxlen - 1 && src[i]; i++) {
        dst[i] = src[i];
    }
    dst[i] = '\0';
}

typedef struct {
    char name[32];
    const char* content;
    uint64_t size;
} fat32_demo_file_t;

static const char user_profile_text[] =
    "[USER_PROFILE]\n"
    "username = phoxtra_admin\n"
    "role = system_operator\n"
    "home_directory = /home\n"
    "identity_status = verified_secure\n";

static uint64_t fat32_file_read(vfs_node_t* node, uint64_t offset, uint64_t size, uint8_t* buffer) {
    if (!node || !node->ptr || !buffer) return 0;
    fat32_demo_file_t* file = (fat32_demo_file_t*)node->ptr;

    if (offset >= file->size) return 0;
    uint64_t bytes = size;
    if (offset + bytes > file->size) bytes = file->size - offset;

    const char* src = file->content + offset;
    for (uint64_t i = 0; i < bytes; i++) {
        buffer[i] = (uint8_t)src[i];
    }
    return bytes;
}

static vfs_node_t* fat32_dir_readdir(vfs_node_t* node, uint32_t index) {
    if (!node) return (void*)0;
    vfs_node_t* child = node->first_child;
    uint32_t curr = 0;

    while (child) {
        if (curr == index) return child;
        child = child->next_sibling;
        curr++;
    }
    return (void*)0;
}

vfs_node_t* fat32_mount(block_device_t* dev, const char* mount_point) {
    if (!dev) return (void*)0;

    klog(LOG_INFO, "FAT32: Mounting volume to VFS target");

    vfs_node_t* dir = vfs_create_node(mount_point + 1, VFS_DIRECTORY);
    if (!dir) return (void*)0;
    dir->readdir = fat32_dir_readdir;

    fat32_demo_file_t* f1 = (fat32_demo_file_t*)kmalloc(sizeof(fat32_demo_file_t));
    kstrncpy(f1->name, "user.txt", sizeof(f1->name));
    f1->content = user_profile_text;
    f1->size    = sizeof(user_profile_text) - 1;

    vfs_node_t* n1 = vfs_create_node("user.txt", VFS_FILE);
    if (n1) {
        n1->size = f1->size;
        n1->read = fat32_file_read;
        n1->ptr  = f1;

        dir->first_child = n1;
        n1->parent       = dir;
    }

    vfs_mount(mount_point, dir);
    return dir;
}

void fat32_init(void) {
    klog(LOG_INFO, "FAT32 demo adapter initialized; on-disk FAT32 parsing is not implemented");
    block_device_t* p3 = blockdev_get("ramdisk0p3");
    if (p3) fat32_mount(p3, "/home");
}
