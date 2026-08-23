#include "vfs.h"
#include "../memory/heap.h"
#include "../log/logger.h"

static vfs_node_t* vfs_root = (void*)0;

static void kstrncpy(char* dst, const char* src, int maxlen) {
    int i;
    for (i = 0; i < maxlen - 1 && src[i]; i++)
        dst[i] = src[i];
    dst[i] = '\0';
}

static int kstrcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) { s1++; s2++; }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

vfs_node_t* vfs_create_node(const char* name, uint32_t flags) {
    vfs_node_t* node = (vfs_node_t*)kmalloc(sizeof(vfs_node_t));
    if (!node) {
        klog(LOG_ERROR, "VFS: Failed to allocate memory for node");
        return (void*)0;
    }
    for (size_t i = 0; i < sizeof(vfs_node_t); i++)
        ((uint8_t*)node)[i] = 0;

    kstrncpy(node->name, name, 64);
    node->flags = flags;
    return node;
}

void vfs_init(void) {
    vfs_root = vfs_create_node("/", VFS_DIRECTORY);
    klog(LOG_KERNEL, "Virtual File System (VFS) initialized");
}

vfs_node_t* vfs_get_root(void) {
    return vfs_root;
}

int vfs_mount(const char* path, vfs_node_t* node) {
    if (!vfs_root || !node) return -1;
    if (path[0] == '/' && path[1] == '\0') {
        vfs_root = node;
        return 0;
    }

    // Fast path for top-level root mounting (e.g. /dev or /initrd)
    const char* child_name = (path[0] == '/') ? path + 1 : path;
    kstrncpy(node->name, child_name, 64);
    node->parent = vfs_root;

    if (!vfs_root->first_child) {
        vfs_root->first_child = node;
    } else {
        vfs_node_t* curr = vfs_root->first_child;
        while (curr->next_sibling) {
            curr = curr->next_sibling;
        }
        curr->next_sibling = node;
    }
    return 0;
}

vfs_node_t* vfs_lookup(const char* path) {
    if (!vfs_root || !path) return (void*)0;
    if (kstrcmp(path, "/") == 0) return vfs_root;

    const char* target = (path[0] == '/') ? path + 1 : path;

    // Check direct children of root
    vfs_node_t* child = vfs_root->first_child;
    while (child) {
        if (kstrcmp(child->name, target) == 0) {
            return child;
        }
        // Subdirectory check (e.g. dev/console or initrd/welcome.txt)
        int len = 0;
        while (target[len] && target[len] != '/') len++;
        if (target[len] == '/') {
            char dir_name[64];
            for (int i = 0; i < len && i < 63; i++) dir_name[i] = target[i];
            dir_name[len] = '\0';
            if (kstrcmp(child->name, dir_name) == 0 && child->first_child) {
                vfs_node_t* sub = child->first_child;
                const char* sub_target = target + len + 1;
                while (sub) {
                    if (kstrcmp(sub->name, sub_target) == 0) return sub;
                    sub = sub->next_sibling;
                }
            }
        }
        child = child->next_sibling;
    }
    return (void*)0;
}

uint64_t vfs_read(vfs_node_t* node, uint64_t offset, uint64_t size, uint8_t* buffer) {
    if (!node || !node->read) return 0;
    return node->read(node, offset, size, buffer);
}

uint64_t vfs_write(vfs_node_t* node, uint64_t offset, uint64_t size, const uint8_t* buffer) {
    if (!node || !node->write) return 0;
    return node->write(node, offset, size, buffer);
}

vfs_node_t* vfs_readdir(vfs_node_t* node, uint32_t index) {
    if (!node) return (void*)0;
    if (node->readdir) return node->readdir(node, index);

    vfs_node_t* child = node->first_child;
    uint32_t curr = 0;
    while (child) {
        if (curr == index) return child;
        child = child->next_sibling;
        curr++;
    }
    return (void*)0;
}
