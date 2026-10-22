#include "file_manager.h"
#include "compositor.h"
#include "font.h"
#include "theme.h"
#include "../drivers/framebuffer.h"
#include "../vfs/vfs.h"
#include "../log/logger.h"

static window_t* fm_win = 0;
static char current_path[128] = "/home/user";
static vfs_node_t* current_directory = 0;
static uint32_t visible_entries = 0;

static int copy_text(char* dst, const char* src, uint32_t cap) {
    if (!dst || !src || !cap) return 0;
    uint32_t i = 0;
    while (src[i] && i + 1 < cap) { dst[i] = src[i]; ++i; }
    dst[i] = '\0';
    return src[i] == '\0';
}
static void redraw_entries(void) {
    if (!fm_win) return;
    theme_palette_t* palette = theme_get_current();
    fb_fill_rect(fm_win->x + 8, fm_win->y + 36, fm_win->width - 16, fm_win->height - 44, fm_win->bg_color);
    font_draw_string(current_path, fm_win->x + 16, fm_win->y + 42, palette->accent_secondary, 0);
    fb_fill_rect(fm_win->x + 16, fm_win->y + 62, fm_win->width - 32, 2, 0x00334155);
    current_directory = vfs_lookup(current_path);
    visible_entries = 0;
    if (!current_directory || !(current_directory->flags & VFS_DIRECTORY)) {
        font_draw_string("Directory unavailable", fm_win->x + 16, fm_win->y + 82, palette->text_color, 0);
        return;
    }
    for (uint32_t i = 0; i < 6; ++i) {
        vfs_node_t* node = vfs_readdir(current_directory, i);
        if (!node) break;
        uint32_t col = i % 3, row = i / 3;
        uint32_t x = fm_win->x + 22 + col * 112;
        uint32_t y = fm_win->y + 78 + row * 92;
        if (node->flags & VFS_DIRECTORY) icon_draw_folder(x, y, node->name);
        else icon_draw_file(x, y, node->name);
        ++visible_entries;
    }
    if (!visible_entries) font_draw_string("This directory is empty", fm_win->x + 16, fm_win->y + 84, palette->text_color, 0);
    font_draw_string("Double-click an entry to open; Backspace goes up", fm_win->x + 16, fm_win->y + fm_win->height - 24, palette->text_color, 0);
}
void file_manager_init(void) {
    klog(LOG_INFO, "Initializing VFS-backed Phoxtra File Manager");
}
void file_manager_open_window(const char* initial_path) {
    if (initial_path && initial_path[0]) copy_text(current_path, initial_path, sizeof(current_path));
    if (!vfs_lookup(current_path)) copy_text(current_path, "/home/user", sizeof(current_path));
    fm_win = compositor_create_window("PHOXTRA FILE MANAGER", 150, 90, 640, 390, 0x00020617);
    if (!fm_win) return;
    redraw_entries();
    klog(LOG_INFO, "Opened VFS-backed File Manager");
}
void file_manager_navigate(const char* path) {
    if (!path || !path[0]) return;
    vfs_node_t* node = vfs_lookup(path);
    if (!node || !(node->flags & VFS_DIRECTORY)) return;
    if (!copy_text(current_path, path, sizeof(current_path))) return;
    redraw_entries();
}
int file_manager_open_entry(uint32_t index) {
    if (!current_directory) current_directory = vfs_lookup(current_path);
    vfs_node_t* node = vfs_readdir(current_directory, index);
    if (!node) return 0;
    if (node->flags & VFS_DIRECTORY) {
        char next[128];
        uint32_t p = 0;
        while (current_path[p] && p < sizeof(next) - 2) { next[p] = current_path[p]; ++p; }
        if (p > 1 && next[p - 1] != '/') next[p++] = '/';
        for (uint32_t i = 0; node->name[i] && p < sizeof(next) - 1; ++i) next[p++] = node->name[i];
        next[p] = '\0';
        file_manager_navigate(next);
        return 1;
    }
    uint8_t buffer[96];
    uint64_t got = vfs_read(node, 0, sizeof(buffer) - 1, buffer);
    buffer[got < sizeof(buffer) ? got : sizeof(buffer) - 1] = 0;
    klog(LOG_INFO, "File selected in file manager");
    return 1;
}
void file_manager_create_folder(const char* name) {
    if (!name || !name[0]) return;
    if (!current_directory) current_directory = vfs_lookup(current_path);
    if (vfs_create_child(current_directory, name, VFS_DIRECTORY)) {
        redraw_entries();
        klog(LOG_INFO, "File Manager: Created RAM-backed directory");
    } else klog(LOG_WARNING, "File Manager: Directory creation failed");
}
void file_manager_create_file(const char* name, const char* content) {
    if (!name || !name[0]) return;
    if (!current_directory) current_directory = vfs_lookup(current_path);
    if (vfs_create_ram_file(current_directory, name, content ? content : "")) {
        redraw_entries();
        klog(LOG_INFO, "File Manager: Created RAM-backed file");
    } else klog(LOG_WARNING, "File Manager: file creation failed");
}
