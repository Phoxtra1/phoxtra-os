#include "vfs.h"
#include "../memory/heap.h"
#include "../log/logger.h"

static vfs_node_t* vfs_root = (void*)0;

static void kstrncpy(char* dst, const char* src, int maxlen) {
    int i = 0;
    if (!dst || maxlen <= 0) return;
    if (src) {
        while (i < maxlen - 1 && src[i]) { dst[i] = src[i]; ++i; }
    }
    dst[i] = '\0';
}
static int kstrcmp(const char* a, const char* b) {
    if (!a || !b) return (a != b);
    while (*a && *a == *b) { ++a; ++b; }
    return (int)(uint8_t)*a - (int)(uint8_t)*b;
}

vfs_node_t* vfs_create_node(const char* name, uint32_t flags) {
    if (!name || !name[0]) return (void*)0;
    vfs_node_t* node = (vfs_node_t*)kmalloc(sizeof(vfs_node_t));
    if (!node) {
        klog(LOG_ERROR, "VFS: Failed to allocate memory for node");
        return (void*)0;
    }
    uint8_t* bytes = (uint8_t*)node;
    for (size_t i = 0; i < sizeof(vfs_node_t); ++i) bytes[i] = 0;
    kstrncpy(node->name, name, (int)sizeof(node->name));
    node->flags = flags;
    return node;
}

vfs_node_t* vfs_create_child(vfs_node_t* parent, const char* name, uint32_t flags) {
    if (!parent || !(parent->flags & VFS_DIRECTORY) || !name || !name[0]) return (void*)0;
    if (kstrcmp(name, ".") == 0 || kstrcmp(name, "..") == 0) return (void*)0;
    for (uint32_t i = 0; name[i]; ++i) {
        if (name[i] == '/' || i >= sizeof(((vfs_node_t*)0)->name) - 1) return (void*)0;
    }
    vfs_node_t* existing = parent->first_child;
    while (existing) {
        if (kstrcmp(existing->name, name) == 0) return (void*)0;
        existing = existing->next_sibling;
    }
    vfs_node_t* node = vfs_create_node(name, flags);
    if (!node) return (void*)0;
    node->parent = parent;
    if (!parent->first_child) {
        parent->first_child = node;
    } else {
        vfs_node_t* last = parent->first_child;
        while (last->next_sibling) last = last->next_sibling;
        last->next_sibling = node;
    }
    return node;
}

void vfs_init(void) {
    vfs_root = vfs_create_node("/", VFS_DIRECTORY);
    if (vfs_root) klog(LOG_KERNEL, "Virtual File System (VFS) initialized");
}

vfs_node_t* vfs_get_root(void) { return vfs_root; }

int vfs_mount(const char* path, vfs_node_t* node) {
    if (!vfs_root || !node || !path || !path[0]) return -1;
    if (kstrcmp(path, "/") == 0) {
        if (!(node->flags & VFS_DIRECTORY)) return -1;
        vfs_root = node;
        node->parent = node;
        return 0;
    }
    const char* name = path;
    while (*name == '/') ++name;
    if (!*name || name[0] == '.' || kstrcmp(name, "..") == 0) return -1;
    for (uint32_t i = 0; name[i]; ++i) if (name[i] == '/') return -1;
    vfs_node_t* mounted = vfs_create_child(vfs_root, name, node->flags);
    if (!mounted) return -1;
    /* Replace the just-created placeholder with the real node in-place. */
    vfs_node_t* previous = (void*)0;
    vfs_node_t* curr = vfs_root->first_child;
    while (curr && curr != mounted) { previous = curr; curr = curr->next_sibling; }
    if (!curr) return -1;
    node->parent = vfs_root;
    node->next_sibling = mounted->next_sibling;
    if (previous) previous->next_sibling = node;
    else vfs_root->first_child = node;
    kfree(mounted);
    kstrncpy(node->name, name, (int)sizeof(node->name));
    return 0;
}

vfs_node_t* vfs_lookup(const char* path) {
    if (!vfs_root || !path) return (void*)0;
    vfs_node_t* node = vfs_root;
    const char* cursor = path;
    while (*cursor == '/') ++cursor;
    if (!*cursor) return node;

    while (*cursor) {
        while (*cursor == '/') ++cursor;
        if (!*cursor) break;
        const char* start = cursor;
        while (*cursor && *cursor != '/') ++cursor;
        size_t len = (size_t)(cursor - start);
        if (!len) continue;
        if (len >= sizeof(((vfs_node_t*)0)->name)) return (void*)0;

        char component[64];
        for (size_t i = 0; i < len; ++i) component[i] = start[i];
        component[len] = '\0';

        if (kstrcmp(component, ".") == 0) continue;
        if (kstrcmp(component, "..") == 0) {
            if (node->parent && node != vfs_root) node = node->parent;
            continue;
        }
        if (!(node->flags & VFS_DIRECTORY)) return (void*)0;
        vfs_node_t* child = node->first_child;
        while (child && kstrcmp(child->name, component) != 0) child = child->next_sibling;
        if (!child) return (void*)0;
        node = child;
    }
    return node;
}

uint64_t vfs_read(vfs_node_t* node, uint64_t offset, uint64_t size, uint8_t* buffer) {
    if (!node || !node->read || !buffer || size == 0) return 0;
    return node->read(node, offset, size, buffer);
}
uint64_t vfs_write(vfs_node_t* node, uint64_t offset, uint64_t size, const uint8_t* buffer) {
    if (!node || !node->write || !buffer || size == 0) return 0;
    return node->write(node, offset, size, buffer);
}
vfs_node_t* vfs_readdir(vfs_node_t* node, uint32_t index) {
    if (!node || !(node->flags & VFS_DIRECTORY)) return (void*)0;
    if (node->readdir) return node->readdir(node, index);
    vfs_node_t* child = node->first_child;
    while (child && index) { child = child->next_sibling; --index; }
    return child;
}

static vfs_node_t* ensure_directory(vfs_node_t* parent, const char* name) {
    vfs_node_t* child = parent ? parent->first_child : (void*)0;
    while (child) {
        if (kstrcmp(child->name, name) == 0)
            return (child->flags & VFS_DIRECTORY) ? child : (void*)0;
        child = child->next_sibling;
    }
    return vfs_create_child(parent, name, VFS_DIRECTORY);
}

void vfs_seed_default_tree(void) {
    if (!vfs_root) return;
    vfs_node_t* home = ensure_directory(vfs_root, "home");
    vfs_node_t* user = ensure_directory(home, "user");
    if (!user) {
        klog(LOG_WARNING, "VFS: user workspace could not be initialized");
        return;
    }
    (void)ensure_directory(user, "Desktop");
    (void)ensure_directory(user, "Documents");
    (void)ensure_directory(user, "Downloads");
    (void)ensure_directory(user, "Pictures");
    (void)ensure_directory(user, "Music");
    klog(LOG_INFO, "VFS seeded a temporary RAM-backed home workspace");
}
