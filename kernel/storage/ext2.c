#include "ext2.h"
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
} ext2_demo_file_t;

static const char boot_cfg_text[] =
    "TIMEOUT 5\n"
    "DEFAULT phoxtra\n\n"
    "LABEL phoxtra\n"
    "    KERNEL /boot/phoxtra\n"
    "    APPEND root=/dev/sata0p1 rw loglevel=info\n";

static const char sys_config_text[] =
    "[PHOXTRA_SYSTEM_CONFIG]\n"
    "kernel.version = 0.7.0\n"
    "storage.ahci = enabled\n"
    "security.tpm = auto\n"
    "fs.root = ext2\n";

static uint64_t ext2_file_read(vfs_node_t* node, uint64_t offset, uint64_t size, uint8_t* buffer) {
    if (!node || !node->ptr || !buffer) return 0;
    ext2_demo_file_t* file = (ext2_demo_file_t*)node->ptr;

    if (offset >= file->size) return 0;
    uint64_t bytes = size;
    if (offset + bytes > file->size) bytes = file->size - offset;

    const char* src = file->content + offset;
    for (uint64_t i = 0; i < bytes; i++) {
        buffer[i] = (uint8_t)src[i];
    }
    return bytes;
}

static vfs_node_t* ext2_dir_readdir(vfs_node_t* node, uint32_t index) {
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

vfs_node_t* ext2_mount(block_device_t* dev, const char* mount_point) {
    if (!dev) return (void*)0;

    klog(LOG_INFO, "Ext2: Mounting block device to VFS target");

    vfs_node_t* dir = vfs_create_node(mount_point + 1, VFS_DIRECTORY);
    if (!dir) return (void*)0;
    dir->readdir = ext2_dir_readdir;

    if (mount_point[1] == 'b') { // /boot
        ext2_demo_file_t* f1 = (ext2_demo_file_t*)kmalloc(sizeof(ext2_demo_file_t));
        kstrncpy(f1->name, "phoxtra.cfg", sizeof(f1->name));
        f1->content = boot_cfg_text;
        f1->size    = sizeof(boot_cfg_text) - 1;

        vfs_node_t* n1 = vfs_create_node("phoxtra.cfg", VFS_FILE);
        if (n1) {
            n1->size = f1->size;
            n1->read = ext2_file_read;
            n1->ptr  = f1;

            dir->first_child = n1;
            n1->parent       = dir;
        }
    } else { // /usr or default
        ext2_demo_file_t* f2 = (ext2_demo_file_t*)kmalloc(sizeof(ext2_demo_file_t));
        kstrncpy(f2->name, "system.conf", sizeof(f2->name));
        f2->content = sys_config_text;
        f2->size    = sizeof(sys_config_text) - 1;

        vfs_node_t* n2 = vfs_create_node("system.conf", VFS_FILE);
        if (n2) {
            n2->size = f2->size;
            n2->read = ext2_file_read;
            n2->ptr  = f2;

            dir->first_child = n2;
            n2->parent       = dir;
        }
    }

    vfs_mount(mount_point, dir);
    return dir;
}

void ext2_init(void) {
    klog(LOG_INFO, "Ext2 Filesystem Driver Subsystem Initialized");
    block_device_t* p1 = blockdev_get("sata0p1");
    if (p1) ext2_mount(p1, "/boot");

    block_device_t* p2 = blockdev_get("sata0p2");
    if (p2) ext2_mount(p2, "/usr");
}
