#include "ext2.h"
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
} ext2_demo_file_t;

static const char boot_cfg_text[] =
    "TIMEOUT 5\n"
    "DEFAULT phoxtra\n\n"
    "LABEL phoxtra\n"
    "    KERNEL /boot/phoxtra\n"
    "    APPEND root=/dev/ramdisk0p1 ro loglevel=info\n";

static const char sys_config_text[] =
    "[PHOXTRA_SYSTEM_CONFIG]\n"
    "kernel.version = 0.7.0\n"
    "storage.backend = volatile_ram_demo\n"
    "security.tpm = status_reported_separately\n"
    "fs.root = in_memory_demo_adapter\n";

static uint64_t ext2_file_read(vfs_node_t* node, uint64_t offset, uint64_t size, uint8_t* buffer) {
    if (!node || !node->ptr || !buffer) return 0;
    ext2_demo_file_t* file = (ext2_demo_file_t*)node->ptr;
    if (!file->content || offset >= file->size || size == 0) return 0;

    /* Subtract only after offset is validated, avoiding offset + size overflow. */
    uint64_t bytes = size;
    if (bytes > file->size - offset) bytes = file->size - offset;
    const char* src = file->content + offset;
    for (uint64_t i = 0; i < bytes; ++i) buffer[i] = (uint8_t)src[i];
    return bytes;
}

static vfs_node_t* ext2_dir_readdir(vfs_node_t* node, uint32_t index) {
    if (!node) return (void*)0;
    vfs_node_t* child = node->first_child;
    while (child && index) { child = child->next_sibling; --index; }
    return child;
}

vfs_node_t* ext2_mount(block_device_t* dev, const char* mount_point) {
    if (!dev || !mount_point || !(dev->flags & BLOCKDEV_FLAG_SIMULATED) ||
        (!strings_equal(mount_point, "/boot") && !strings_equal(mount_point, "/usr"))) {
        klog(LOG_WARNING, "Ext2 demo adapter: rejected invalid device or mount target");
        return (void*)0;
    }
    if (vfs_lookup(mount_point)) {
        klog(LOG_WARNING, "Ext2 demo adapter: mount target already exists");
        return (void*)0;
    }

    const int is_boot = strings_equal(mount_point, "/boot");
    const char* file_name = is_boot ? "phoxtra.cfg" : "system.conf";
    const char* content = is_boot ? boot_cfg_text : sys_config_text;

    vfs_node_t* dir = vfs_create_node(mount_point + 1, VFS_DIRECTORY);
    if (!dir) return (void*)0;
    dir->readdir = ext2_dir_readdir;

    ext2_demo_file_t* file = (ext2_demo_file_t*)kmalloc(sizeof(*file));
    if (!file) {
        kfree(dir);
        klog(LOG_ERROR, "Ext2 demo adapter: file metadata allocation failed");
        return (void*)0;
    }
    kstrncpy(file->name, file_name, (int)sizeof(file->name));
    file->content = content;
    file->size = is_boot ? sizeof(boot_cfg_text) - 1u : sizeof(sys_config_text) - 1u;

    vfs_node_t* node = vfs_create_node(file_name, VFS_FILE);
    if (!node) {
        kfree(file);
        kfree(dir);
        return (void*)0;
    }
    node->size = file->size;
    node->read = ext2_file_read;
    node->ptr = file;
    node->parent = dir;
    dir->first_child = node;

    if (vfs_mount(mount_point, dir) != 0) {
        kfree(node);
        kfree(file);
        kfree(dir);
        klog(LOG_WARNING, "Ext2 demo adapter: VFS mount failed");
        return (void*)0;
    }

    klog(LOG_INFO, "Ext2 demo adapter mounted in-memory sample content; on-disk ext2 parsing is not implemented");
    return dir;
}

void ext2_init(void) {
    klog(LOG_INFO, "Ext2 demo adapter initialized; on-disk ext2 parsing is not implemented");
    block_device_t* p1 = blockdev_get("ramdisk0p1");
    block_device_t* p2 = blockdev_get("ramdisk0p2");
    if (p1) (void)ext2_mount(p1, "/boot");
    if (p2) (void)ext2_mount(p2, "/usr");
}
