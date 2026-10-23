#include "platform.h"
#include "../include/keyboard.h"
#include "../src/mouse.h"
#include "../drivers/framebuffer.h"
#include "../vfs/vfs.h"
#include "../drivers/pci.h"
#include "../security/mac.h"
#include "../security/capability.h"
#include "../log/logger.h"

#define PLATFORM_APP_COUNT 14u

static uint8_t initialized;
static uint32_t status_flags;

#ifndef PHOXTRA_PRODUCT_KIND
#define PHOXTRA_PRODUCT_KIND PLATFORM_PROFILE_LAPTOP
#endif

#if PHOXTRA_PRODUCT_KIND == 3
static const platform_product_profile_t active_profile = {
    PLATFORM_PROFILE_EMBEDDED, "phoxtra-embedded", "Phoxtra Embedded Device", 0, 1, 1
};
#elif PHOXTRA_PRODUCT_KIND == 2
static const platform_product_profile_t active_profile = {
    PLATFORM_PROFILE_DESKTOP, "phoxtra-desktop", "Phoxtra Desktop", 1, 1, 1
};
#else
static const platform_product_profile_t active_profile = {
    PLATFORM_PROFILE_LAPTOP, "phoxtra-laptop", "Phoxtra Laptop", 1, 1, 1
};
#endif

static const platform_app_info_t app_registry[PLATFORM_APP_COUNT] = {
    /* Phoxtra OS native panels and first-party system applications. */
    { "terminal", "Shell Console", "Phoxtra", "System", "Native panel", "Kernel shell access; desktop terminal emulation is not yet implemented", "panel:terminal", PLATFORM_PUBLISHER_PHOXTRA, PLATFORM_APP_EMBEDDED_PANEL, PLATFORM_LAUNCH_INTERNAL_PANEL, PLATFORM_APP_AVAILABLE },
    { "file-manager", "Files", "Phoxtra", "Files and storage", "Native panel", "Browse the current RAM-backed filesystem", "panel:file-manager", PLATFORM_PUBLISHER_PHOXTRA, PLATFORM_APP_EMBEDDED_PANEL, PLATFORM_LAUNCH_INTERNAL_PANEL, PLATFORM_APP_AVAILABLE },
    { "system-status", "System Status", "Phoxtra", "System tools", "Native panel", "View current OS, input and storage status", "panel:system-status", PLATFORM_PUBLISHER_PHOXTRA, PLATFORM_APP_EMBEDDED_PANEL, PLATFORM_LAUNCH_INTERNAL_PANEL, PLATFORM_APP_AVAILABLE },
    { "settings", "Settings", "Phoxtra", "System tools", "Planned native application", "Unified device and desktop settings; implementation planned", NULL, PLATFORM_PUBLISHER_PHOXTRA, PLATFORM_APP_PLANNED_APPLICATION, PLATFORM_LAUNCH_PENDING, PLATFORM_APP_PLANNED },
    { "phoxtra-diagnostics", "Phoxtra Diagnostics", "Phoxtra", "System tools", "Planned native application", "System diagnostics application; full interactive panel planned", NULL, PLATFORM_PUBLISHER_PHOXTRA, PLATFORM_APP_PLANNED_APPLICATION, PLATFORM_LAUNCH_PENDING, PLATFORM_APP_PLANNED },
    /* Amineva Global products. */
    { "agroclimate", "AgroClimate", "Amineva Global", "Agriculture and climate intelligence", "External service", "Agriculture and climate intelligence product; integration pending", NULL, PLATFORM_PUBLISHER_AMINEVA_GLOBAL, PLATFORM_APP_PLANNED_APPLICATION, PLATFORM_LAUNCH_PENDING, PLATFORM_APP_PLANNED },
    { "adesua", "Adesua", "Amineva Global", "Learning and practical intelligence", "External service", "Practical and formula intelligence learning platform; integration pending", NULL, PLATFORM_PUBLISHER_AMINEVA_GLOBAL, PLATFORM_APP_PLANNED_APPLICATION, PLATFORM_LAUNCH_PENDING, PLATFORM_APP_PLANNED },
    { "datalens", "DataLens", "Amineva Global", "IT services and data management", "External service", "IT services and data-management product; integration pending", NULL, PLATFORM_PUBLISHER_AMINEVA_GLOBAL, PLATFORM_APP_PLANNED_APPLICATION, PLATFORM_LAUNCH_PENDING, PLATFORM_APP_PLANNED },
    { "inventra", "Inventra", "Amineva Global", "Inventory management", "External service", "Inventory management product; integration pending", NULL, PLATFORM_PUBLISHER_AMINEVA_GLOBAL, PLATFORM_APP_PLANNED_APPLICATION, PLATFORM_LAUNCH_PENDING, PLATFORM_APP_PLANNED },
    { "sikasem", "Sikasem", "Amineva Global", "Financial operations", "External service", "Financial operating system product; integration pending", NULL, PLATFORM_PUBLISHER_AMINEVA_GLOBAL, PLATFORM_APP_PLANNED_APPLICATION, PLATFORM_LAUNCH_PENDING, PLATFORM_APP_PLANNED },
    { "amineva-workspace", "Amineva Workspace", "Amineva Global", "Productivity suite", "Integrated productivity suite", "Docs, Sheets, Slides, Forms and Drive; desktop integration pending", NULL, PLATFORM_PUBLISHER_AMINEVA_GLOBAL, PLATFORM_APP_PLANNED_APPLICATION, PLATFORM_LAUNCH_PENDING, PLATFORM_APP_PLANNED },
    /* Independent publisher: Amnewsworld is not an Amineva Global product. */
    { "amnewsworld-news", "Amnewsworld News", "Amnewsworld", "News and publishing", "External service", "Independent news portal; launch hand-off is not configured", NULL, PLATFORM_PUBLISHER_AMNEWSWORLD, PLATFORM_APP_PLANNED_APPLICATION, PLATFORM_LAUNCH_PENDING, PLATFORM_APP_PLANNED },
    { "amnewsworld-radio", "Amnewsworld Radio and Media", "Amnewsworld", "Radio and media production", "External service", "Independent web radio and media-production services; launch hand-off is not configured", NULL, PLATFORM_PUBLISHER_AMNEWSWORLD, PLATFORM_APP_PLANNED_APPLICATION, PLATFORM_LAUNCH_PENDING, PLATFORM_APP_PLANNED },
    { "app-marketplace", "App Marketplace", "Third-party developers", "Apps and extensions", "Planned marketplace", "Discover applications from Phoxtra and independent developers; install flow not implemented", NULL, PLATFORM_PUBLISHER_THIRD_PARTY, PLATFORM_APP_PLANNED_APPLICATION, PLATFORM_LAUNCH_CATALOGUE, PLATFORM_APP_PLANNED }
};

static int text_equal(const char* a, const char* b) {
    if (!a || !b) return 0;
    while (*a && *a == *b) { ++a; ++b; }
    return *a == *b;
}

static void copy_name(char* dst, const char* src, uint32_t cap) {
    if (!dst || !cap) return;
    uint32_t i = 0;
    if (src) while (src[i] && i + 1 < cap) { dst[i] = src[i]; ++i; }
    dst[i] = '\0';
}

int platform_services_init(void) {
    framebuffer_t* fb = framebuffer_get();
    if (fb && fb->available) status_flags |= PLATFORM_STATUS_DISPLAY;
    status_flags |= PLATFORM_STATUS_KEYBOARD;
    if (mouse_is_available()) status_flags |= PLATFORM_STATUS_POINTER;
    if (vfs_get_root()) {
        status_flags |= PLATFORM_STATUS_VFS | PLATFORM_STATUS_STORAGE_VOLATILE;
    }
    if (pci_get_device_count() > 0) status_flags |= PLATFORM_STATUS_PCI_INVENTORY;
    if (PLATFORM_APP_COUNT > 0) status_flags |= PLATFORM_STATUS_APP_REGISTRY;
    status_flags |= PLATFORM_STATUS_POLICY_PROVIDER;
    initialized = 1;
    klog(LOG_INFO, "Phoxtra shared platform services initialized");
    klog(LOG_INFO, active_profile.id);
    klog(LOG_INFO, "Storage backend: volatile RAM VFS; persistent storage is not yet connected");
    return PLATFORM_OK;
}

int platform_services_is_initialized(void) { return initialized != 0; }
uint32_t platform_services_status_flags(void) { return status_flags; }
const platform_product_profile_t* platform_product_profile(void) { return &active_profile; }

int platform_input_poll(platform_input_event_t* event) {
    if (!event) return PLATFORM_ERR_INVALID;
    char key;
    if (keyboard_poll_char(&key)) {
        event->type = PLATFORM_INPUT_KEY;
        event->key = (uint8_t)key;
        event->buttons = 0;
        event->modifiers = keyboard_control_pressed() ? PLATFORM_MOD_CTRL : 0;
        event->x = 0;
        event->y = 0;
        return 1;
    }
    mouse_event_t mouse;
    if (mouse_poll_event(&mouse)) {
        event->type = PLATFORM_INPUT_POINTER;
        event->key = 0;
        event->buttons = (uint8_t)(mouse.buttons & 0x07u);
        if (mouse.left_clicked) event->buttons |= PLATFORM_INPUT_LEFT_CLICKED;
        event->modifiers = keyboard_control_pressed() ? PLATFORM_MOD_CTRL : 0;
        event->x = mouse.x;
        event->y = mouse.y;
        return 1;
    }
    event->type = PLATFORM_INPUT_NONE;
    return 0;
}

int platform_input_pointer_position(int32_t* x, int32_t* y) {
    if (!x || !y) return PLATFORM_ERR_INVALID;
    if (!mouse_is_available()) return PLATFORM_ERR_UNSUPPORTED;
    mouse_get_position(x, y);
    return PLATFORM_OK;
}
int platform_input_has_keyboard(void) { return 1; }
int platform_input_has_pointer(void) { return mouse_is_available(); }

int platform_display_get_info(platform_display_info_t* info) {
    if (!info) return PLATFORM_ERR_INVALID;
    framebuffer_t* fb = framebuffer_get();
    if (!fb || !fb->available) {
        info->width = info->height = info->pitch = info->bits_per_pixel = 0;
        info->available = 0;
        return PLATFORM_ERR_UNSUPPORTED;
    }
    info->width = fb->width;
    info->height = fb->height;
    info->pitch = fb->pitch;
    info->bits_per_pixel = fb->bpp;
    info->available = 1;
    return PLATFORM_OK;
}
int platform_display_is_available(void) { return framebuffer_is_available(); }
void platform_display_put_pixel(uint32_t x, uint32_t y, uint32_t color) { fb_put_pixel(x, y, color); }
void platform_display_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t c) { fb_fill_rect(x, y, w, h, c); }
void platform_display_draw_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t c) { fb_draw_rect(x, y, w, h, c); }
void platform_display_draw_gradient(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t a, uint32_t b) { fb_draw_gradient(x, y, w, h, a, b); }
void platform_display_clear(uint32_t color) { fb_clear(color); }
void platform_display_present(void) { fb_swap_buffers(); }

platform_fs_node_t* platform_storage_lookup(const char* path) {
    if (!path || !path[0]) return 0;
    return vfs_lookup(path);
}
int platform_storage_is_directory(const platform_fs_node_t* node) {
    return node && node->flags == VFS_DIRECTORY;
}
int platform_storage_list(const platform_fs_node_t* directory, uint32_t index, platform_fs_entry_t* entry) {
    if (!directory || !entry || directory->flags != VFS_DIRECTORY) return PLATFORM_ERR_INVALID;
    vfs_node_t* child = vfs_readdir((vfs_node_t*)directory, index);
    if (!child) return PLATFORM_ERR_NOT_FOUND;
    copy_name(entry->name, child->name, sizeof(entry->name));
    entry->type = (child->flags == VFS_DIRECTORY) ? PLATFORM_FS_ENTRY_DIRECTORY :
                  ((child->flags == VFS_CHARDEVICE) ? PLATFORM_FS_ENTRY_DEVICE : PLATFORM_FS_ENTRY_FILE);
    entry->size = child->size;
    entry->handle = child;
    return PLATFORM_OK;
}
uint32_t platform_storage_count(const platform_fs_node_t* directory) {
    uint32_t count = 0;
    if (!directory || directory->flags != VFS_DIRECTORY) return 0;
    while (vfs_readdir((vfs_node_t*)directory, count)) ++count;
    return count;
}
uint64_t platform_storage_read(const platform_fs_node_t* file, uint64_t offset, uint64_t size, uint8_t* buffer) {
    if (!file || !buffer || file->flags != VFS_FILE) return 0;
    return vfs_read((vfs_node_t*)file, offset, size, buffer);
}
uint64_t platform_storage_write(platform_fs_node_t* file, uint64_t offset, uint64_t size, const uint8_t* buffer) {
    if (!file || !buffer || file->flags != VFS_FILE) return 0;
    return vfs_write(file, offset, size, buffer);
}
int platform_storage_create_directory(platform_fs_node_t* parent, const char* name) {
    if (!parent || !name || !name[0] || parent->flags != VFS_DIRECTORY) return PLATFORM_ERR_INVALID;
    return vfs_create_child(parent, name, VFS_DIRECTORY) ? PLATFORM_OK : PLATFORM_ERR_IO;
}
int platform_storage_create_text_file(platform_fs_node_t* parent, const char* name, const char* initial_contents) {
    if (!parent || !name || !name[0] || parent->flags != VFS_DIRECTORY) return PLATFORM_ERR_INVALID;
    return vfs_create_ram_file(parent, name, initial_contents) ? PLATFORM_OK : PLATFORM_ERR_IO;
}
int platform_storage_get_info(platform_storage_info_t* info) {
    if (!info) return PLATFORM_ERR_INVALID;
    info->mode = PLATFORM_STORAGE_VOLATILE_RAM;
    info->persistent = 0;
    info->description = "Volatile RAM-backed VFS; contents are lost at reboot";
    return PLATFORM_OK;
}

uint32_t platform_app_count(void) { return PLATFORM_APP_COUNT; }
int platform_app_get(uint32_t index, const platform_app_info_t** info) {
    if (!info) return PLATFORM_ERR_INVALID;
    if (index >= PLATFORM_APP_COUNT) return PLATFORM_ERR_NOT_FOUND;
    *info = &app_registry[index];
    return PLATFORM_OK;
}
int platform_app_find(const char* id, const platform_app_info_t** info) {
    if (!id || !info) return PLATFORM_ERR_INVALID;
    for (uint32_t i = 0; i < PLATFORM_APP_COUNT; ++i) {
        if (text_equal(id, app_registry[i].id)) { *info = &app_registry[i]; return PLATFORM_OK; }
    }
    return PLATFORM_ERR_NOT_FOUND;
}

uint32_t platform_device_count(void) { return pci_get_device_count(); }
int platform_device_get(uint32_t index, platform_device_info_t* info) {
    if (!info) return PLATFORM_ERR_INVALID;
    pci_device_t* dev = pci_get_device(index);
    if (!dev) return PLATFORM_ERR_NOT_FOUND;
    info->bus = dev->bus;
    info->slot = dev->slot;
    info->function = dev->func;
    info->vendor_id = dev->vendor_id;
    info->device_id = dev->device_id;
    info->class_id = dev->class_id;
    info->subclass = dev->subclass;
    info->class_name = pci_class_to_string(dev->class_id, dev->subclass);
    return PLATFORM_OK;
}

int platform_security_check_vfs(const pcb_t* caller, const platform_fs_node_t* node, uint32_t mode) {
    if (!node) return PLATFORM_ERR_INVALID;
    /* Protected operations must provide a process identity; kernel-owned VFS
       construction helpers do not invoke this user-facing authorization gate. */
    if (!caller) return PLATFORM_ERR_DENIED;
    return mac_check_vfs_access((pcb_t*)caller, node->mac_label, mode) ? PLATFORM_OK : PLATFORM_ERR_DENIED;
}
int platform_security_has_capability(pcb_t* caller, uint64_t capability) {
    if (!caller || capability == 0) return 0;
    return capability_check(caller, capability) != 0;
}
