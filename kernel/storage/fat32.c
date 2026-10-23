#include "fat32.h"
#include "../memory/heap.h"
#include "../log/logger.h"

static void kstrncpy(char* dst, const char* src, int maxlen) {
    if (!dst || !src || maxlen <= 0) return;
    int i = 0;
    for (; i < maxlen - 1 && src[i]; ++i) dst[i] = src[i];
    dst[i] = '\0';
}
static int strings_equal(const char* a, const char* b) {
    if (!a || !b) return 0;
    while (*a && *b && *a == *b) { ++a; ++b; }
    return *a == *b;
}

typedef struct {
    char name[32];
    const char* content;
    uint64_t size;
} fat32_demo_file_t;

static const char user_profile_text[] =
    "[DEMO_USER_PROFILE]\n"
    "username = demo_user\n"
    "role = example_operator\n"
    "home_directory = /home\n"
    "identity_status = demo_only_not_verified\n";

static uint64_t fat32_file_read(vfs_node_t* node, uint64_t offset, uint64_t size, uint8_t* buffer) {
    if (!node || !node->ptr || !buffer) return 0;
    fat32_demo_file_t* file = (fat32_demo_file_t*)node->ptr;
    if (!file->content || offset >= file->size || size == 0) return 0;

    /* Subtract only after offset is validated, avoiding offset + size overflow. */
    uint64_t bytes = size;
    if (bytes > file->size - offset) bytes = file->size - offset;
    const char* src = file->content + offset;
    for (uint64_t i = 0; i < bytes; ++i) buffer[i] = (uint8_t)src[i];
    return bytes;
}

static vfs_node_t* fat32_dir_readdir(vfs_node_t* node, uint32_t index) {
    if (!node) return (void*)0;
    vfs_node_t* child = node->first_child;
    while (child && index) { child = child->next_sibling; --index; }
    return child;
}

vfs_node_t* fat32_mount(block_device_t* dev, const char* mount_point) {
    if (!dev || !(dev->flags & BLOCKDEV_FLAG_SIMULATED) ||
        !strings_equal(mount_point, "/home")) {
        klog(LOG_WARNING, "FAT32 demo adapter: rejected invalid device or mount target");
        return (void*)0;
    }

    vfs_node_t* dir = vfs_lookup(mount_point);
    int mount_new_directory = 0;
    if (dir) {
        if (dir->flags != VFS_DIRECTORY) {
            klog(LOG_WARNING, "FAT32 demo adapter: existing mount target is not a directory");
            return (void*)0;
        }
    } else {
        dir = vfs_create_node("home", VFS_DIRECTORY);
        if (!dir) return (void*)0;
        dir->readdir = fat32_dir_readdir;
        mount_new_directory = 1;
    }

    fat32_demo_file_t* file = (fat32_demo_file_t*)kmalloc(sizeof(*file));
    if (!file) {
        if (mount_new_directory) kfree(dir);
        klog(LOG_ERROR, "FAT32 demo adapter: file metadata allocation failed");
        return (void*)0;
    }
    kstrncpy(file->name, "user.txt", (int)sizeof(file->name));
    file->content = user_profile_text;
    file->size = sizeof(user_profile_text) - 1u;

    vfs_node_t* node;
    if (mount_new_directory) {
        node = vfs_create_node("user.txt", VFS_FILE);
        if (node) {
            node->parent = dir;
            dir->first_child = node;
        }
    } else {
        node = vfs_create_child(dir, "user.txt", VFS_FILE);
    }
    if (!node) {
        kfree(file);
        if (mount_new_directory) kfree(dir);
        klog(LOG_WARNING, "FAT32 demo adapter: could not create profile entry (duplicate name or allocation failure)");
        return (void*)0;
    }

    node->size = file->size;
    node->read = fat32_file_read;
    node->ptr = file;
    if (mount_new_directory && vfs_mount(mount_point, dir) != 0) {
        kfree(node);
        kfree(file);
        kfree(dir);
        klog(LOG_WARNING, "FAT32 demo adapter: VFS mount failed");
        return (void*)0;
    }

    klog(LOG_INFO, "FAT32 demo adapter added in-memory sample content; on-disk FAT32 parsing is not implemented");
    return dir;
}

void fat32_init(void) {
    klog(LOG_INFO, "FAT32 demo adapter initialized; on-disk FAT32 parsing is not implemented");
    block_device_t* p3 = blockdev_get("ramdisk0p3");
    if (p3) (void)fat32_mount(p3, "/home");
}
