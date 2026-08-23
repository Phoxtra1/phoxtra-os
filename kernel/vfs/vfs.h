#ifndef PHOXTRA_VFS_H
#define PHOXTRA_VFS_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define VFS_FILE       0x01
#define VFS_DIRECTORY  0x02
#define VFS_CHARDEVICE 0x03

typedef struct vfs_node {
    char name[64];
    uint32_t flags;
    uint64_t size;
    uint32_t permissions;
    uint64_t (*read)(struct vfs_node* node, uint64_t offset, uint64_t size, uint8_t* buffer);
    uint64_t (*write)(struct vfs_node* node, uint64_t offset, uint64_t size, const uint8_t* buffer);
    void (*open)(struct vfs_node* node);
    void (*close)(struct vfs_node* node);
    struct vfs_node* (*readdir)(struct vfs_node* node, uint32_t index);
    uint32_t mac_label;
    void* ptr;
    struct vfs_node* parent;
    struct vfs_node* first_child;
    struct vfs_node* next_sibling;
} vfs_node_t;

void vfs_init(void);
vfs_node_t* vfs_get_root(void);
vfs_node_t* vfs_create_node(const char* name, uint32_t flags);
int vfs_mount(const char* path, vfs_node_t* node);
vfs_node_t* vfs_lookup(const char* path);
uint64_t vfs_read(vfs_node_t* node, uint64_t offset, uint64_t size, uint8_t* buffer);
uint64_t vfs_write(vfs_node_t* node, uint64_t offset, uint64_t size, const uint8_t* buffer);
vfs_node_t* vfs_readdir(vfs_node_t* node, uint32_t index);

#ifdef __cplusplus
}
#endif

#endif
