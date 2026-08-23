#include "devfs.h"
#include "../log/logger.h"

extern void console_write(const char* text);
extern char keyboard_get_char(void);

static uint64_t console_write_cb(vfs_node_t* node, uint64_t offset, uint64_t size, const uint8_t* buffer) {
    (void)node; (void)offset;
    for (uint64_t i = 0; i < size; i++) {
        char str[2] = {(char)buffer[i], '\0'};
        console_write(str);
    }
    return size;
}

static uint64_t keyboard_read_cb(vfs_node_t* node, uint64_t offset, uint64_t size, uint8_t* buffer) {
    (void)node; (void)offset;
    if (size == 0) return 0;
    buffer[0] = (uint8_t)keyboard_get_char();
    return 1;
}

static uint64_t null_read_cb(vfs_node_t* node, uint64_t offset, uint64_t size, uint8_t* buffer) {
    (void)node; (void)offset; (void)buffer; (void)size;
    return 0;
}

static uint64_t null_write_cb(vfs_node_t* node, uint64_t offset, uint64_t size, const uint8_t* buffer) {
    (void)node; (void)offset; (void)buffer;
    return size;
}

void devfs_init(void) {
    vfs_node_t* dev_dir = vfs_create_node("dev", VFS_DIRECTORY);
    vfs_mount("dev", dev_dir);

    vfs_node_t* console_node = vfs_create_node("console", VFS_CHARDEVICE);
    console_node->write = console_write_cb;
    console_node->parent = dev_dir;
    dev_dir->first_child = console_node;

    vfs_node_t* kbd_node = vfs_create_node("keyboard", VFS_CHARDEVICE);
    kbd_node->read = keyboard_read_cb;
    kbd_node->parent = dev_dir;
    console_node->next_sibling = kbd_node;

    vfs_node_t* null_node = vfs_create_node("null", VFS_CHARDEVICE);
    null_node->read = null_read_cb;
    null_node->write = null_write_cb;
    null_node->parent = dev_dir;
    kbd_node->next_sibling = null_node;

    klog(LOG_KERNEL, "DevFS character device filesystem mounted at /dev");
}
