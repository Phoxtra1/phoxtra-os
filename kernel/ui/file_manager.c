#include "file_manager.h"
#include "font.h"
#include "theme.h"
#include "../drivers/framebuffer.h"
#include "../services/platform.h"
#include "../log/logger.h"

#define FM_PATH_CAP 128u
#define FM_NAME_CAP 64u
#define FM_VISIBLE_ITEMS 6u
#define FM_COLUMNS 3u
#define FM_CELL_WIDTH 112u
#define FM_CELL_HEIGHT 92u

static window_t* fm_win = 0;
static window_t* preview_win = 0;
static char current_path[FM_PATH_CAP] = "/home/user";
static platform_fs_node_t* current_directory = 0;
static uint32_t entry_count = 0;
static uint32_t page_start = 0;
static int32_t selected_entry = -1;
static char preview_name[FM_NAME_CAP];
static char preview_text[513];

static int text_copy(char* dst, const char* src, uint32_t capacity) {
    if (!dst || !src || capacity == 0) return 0;
    uint32_t i = 0;
    if (src) while (src[i] && i + 1 < capacity) { dst[i] = src[i]; ++i; }
    dst[i] = '\0';
    return !src || src[i] == '\0';
}
static int text_equal(const char* a, const char* b) {
    if (!a || !b) return a == b;
    while (*a && *a == *b) { ++a; ++b; }
    return *a == *b;
}
static uint32_t count_entries(platform_fs_node_t* dir) {
    return platform_storage_count(dir);
}
static int entry_at(uint32_t index, platform_fs_entry_t* entry) {
    if (!entry || !current_directory || !platform_storage_is_directory(current_directory))
        return 0;
    return platform_storage_list(current_directory, index, entry) == PLATFORM_OK;
}

static void draw_preview(void) {
    if (!preview_win || !preview_win->active || preview_win->minimized) return;
    theme_palette_t* palette = theme_get_current();
    fb_fill_rect(preview_win->x + 10, preview_win->y + 38,
                 preview_win->width - 20, preview_win->height - 48, palette->window_bg);
    font_draw_string(preview_name, preview_win->x + 16, preview_win->y + 42,
                     palette->accent_primary, 0);
    fb_fill_rect(preview_win->x + 14, preview_win->y + 62, preview_win->width - 28, 1, 0x00334155);
    uint32_t x = preview_win->x + 16, y = preview_win->y + 72;
    uint32_t line_chars = (preview_win->width - 32) / (FONT_WIDTH + 1);
    if (!line_chars) line_chars = 1;
    uint32_t chars = 0, lines = 0;
    for (uint32_t i = 0; preview_text[i] && lines < 8; ++i) {
        char ch[2] = { preview_text[i], 0 };
        if (preview_text[i] == '\r') continue;
        if (preview_text[i] == '\n' || chars >= line_chars) {
            y += FONT_HEIGHT + 2;
            x = preview_win->x + 16;
            chars = 0;
            ++lines;
            if (preview_text[i] == '\n') continue;
            --i;
            continue;
        }
        font_draw_string(ch, x, y, palette->text_color, 0);
        x += FONT_WIDTH + 1;
        ++chars;
    }
    font_draw_string("Preview is read-only in this build", preview_win->x + 16,
                     preview_win->y + preview_win->height - 24, 0x0094A3B8, 0);
}

static void redraw_entries(void) {
    if (!fm_win) return;
    theme_palette_t* palette = theme_get_current();
    fb_fill_rect(fm_win->x + 8, fm_win->y + 36, fm_win->width - 16,
                 fm_win->height - 44, palette->window_bg);
    current_directory = platform_storage_lookup(current_path);
    if (!current_directory || !platform_storage_is_directory(current_directory)) {
        font_draw_string("Directory unavailable", fm_win->x + 16, fm_win->y + 44,
                         palette->text_color, 0);
        entry_count = 0;
        return;
    }

    font_draw_string(current_path, fm_win->x + 16, fm_win->y + 42,
                     palette->accent_secondary, 0);
    fb_fill_rect(fm_win->x + 16, fm_win->y + 62, fm_win->width - 32, 2, 0x00334155);
    entry_count = count_entries(current_directory);
    if (page_start >= entry_count && page_start) page_start = ((entry_count - 1) / FM_VISIBLE_ITEMS) * FM_VISIBLE_ITEMS;
    uint32_t visible = entry_count - page_start;
    if (visible > FM_VISIBLE_ITEMS) visible = FM_VISIBLE_ITEMS;

    for (uint32_t i = 0; i < visible; ++i) {
        uint32_t index = page_start + i;
        uint32_t col = i % FM_COLUMNS, row = i / FM_COLUMNS;
        uint32_t x = fm_win->x + 22 + col * FM_CELL_WIDTH;
        uint32_t y = fm_win->y + 78 + row * FM_CELL_HEIGHT;
        platform_fs_entry_t entry;
        if (!entry_at(index, &entry)) continue;
        if ((int32_t)index == selected_entry)
            fb_fill_rect(x - 6, y - 5, FM_CELL_WIDTH - 4, 70, 0x00405267);
        if (entry.type == PLATFORM_FS_ENTRY_DIRECTORY) icon_draw_folder(x, y, entry.name);
        else icon_draw_file(x, y, entry.name);
    }

    if (!entry_count) font_draw_string("This directory is empty", fm_win->x + 16,
                                       fm_win->y + 86, palette->text_color, 0);
    char status[64];
    /* The fixed status text avoids presenting fictitious disk capacity. */
    font_draw_string("Folder: click  File: double-click",
                     fm_win->x + 16, fm_win->y + fm_win->height - 60, palette->text_color, 0);
    font_draw_string("Backspace: parent  N: new folder",
                     fm_win->x + 16, fm_win->y + fm_win->height - 42, palette->text_color, 0);
    font_draw_string(entry_count > FM_VISIBLE_ITEMS
                         ? "C: new file  [ ]: pages"
                         : "C: create file",
                     fm_win->x + 16, fm_win->y + fm_win->height - 24, palette->text_color, 0);
    (void)status;
}

void file_manager_init(void) {
    preview_name[0] = '\0';
    preview_text[0] = '\0';
    klog(LOG_INFO, "Platform-storage file manager initialized");
}
void file_manager_open_window(const char* initial_path) {
    if (initial_path && initial_path[0]) text_copy(current_path, initial_path, sizeof(current_path));
    if (!platform_storage_lookup(current_path)) text_copy(current_path, "/home/user", sizeof(current_path));
    if (fm_win) {
        fm_win->active = 1;
        fm_win->minimized = 0;
        redraw_entries();
        return;
    }
    fm_win = compositor_create_window("PHOXTRA FILE MANAGER", 592, 54, 410, 300, 0x00020617);
    if (!fm_win) return;
    fm_win->active = 1;
    redraw_entries();
    klog(LOG_INFO, "Opened platform-storage File Manager");
}
window_t* file_manager_get_window(void) { return fm_win; }

void file_manager_navigate(const char* path) {
    klog(LOG_INFO, "File manager navigation requested");
    if (!path || !path[0]) return;
    platform_fs_node_t* node = platform_storage_lookup(path);
    if (!platform_storage_is_directory(node)) return;
    if (!text_copy(current_path, path, sizeof(current_path))) return;
    page_start = 0;
    selected_entry = -1;
    current_directory = node;
    redraw_entries();
    klog(LOG_INFO, "File manager navigated to directory");
}
void file_manager_go_parent(void) {
    if (text_equal(current_path, "/")) return;
    char parent[FM_PATH_CAP];
    text_copy(parent, current_path, sizeof(parent));
    uint32_t len = 0;
    while (parent[len]) ++len;
    while (len > 1 && parent[len - 1] == '/') parent[--len] = '\0';
    while (len > 1 && parent[len - 1] != '/') parent[--len] = '\0';
    if (len > 1) parent[len - 1] = '\0';
    if (!parent[0]) text_copy(parent, "/", sizeof(parent));
    file_manager_navigate(parent);
}
void file_manager_scroll(int32_t pages) {
    if (!entry_count) return;
    int32_t next = (int32_t)page_start + pages * (int32_t)FM_VISIBLE_ITEMS;
    if (next < 0) next = 0;
    uint32_t max_page = ((entry_count - 1) / FM_VISIBLE_ITEMS) * FM_VISIBLE_ITEMS;
    if ((uint32_t)next > max_page) next = (int32_t)max_page;
    page_start = (uint32_t)next;
    selected_entry = -1;
    redraw_entries();
}
int file_manager_entry_is_directory(uint32_t index) {
    platform_fs_entry_t entry;
    return entry_at(index, &entry) && entry.type == PLATFORM_FS_ENTRY_DIRECTORY;
}

int file_manager_hit_test(uint32_t x, uint32_t y) {
    if (!fm_win || !fm_win->active || fm_win->minimized) return -1;
    if (x < fm_win->x + 16 || y < fm_win->y + 72 ||
        x >= fm_win->x + fm_win->width - 8 || y >= fm_win->y + fm_win->height - 45) return -1;
    uint32_t col = (x - (fm_win->x + 22)) / FM_CELL_WIDTH;
    uint32_t row = (y - (fm_win->y + 78)) / FM_CELL_HEIGHT;
    if (col >= FM_COLUMNS || row >= 2) return -1;
    uint32_t cell_x = fm_win->x + 22 + col * FM_CELL_WIDTH;
    uint32_t cell_y = fm_win->y + 78 + row * FM_CELL_HEIGHT;
    if (x < cell_x || x >= cell_x + FM_CELL_WIDTH - 4 || y < cell_y - 5 || y >= cell_y + 70) return -1;
    uint32_t index = page_start + row * FM_COLUMNS + col;
    return index < entry_count ? (int)index : -1;
}
void file_manager_select_entry(uint32_t index) {
    if (index >= entry_count) return;
    selected_entry = (int32_t)index;
    redraw_entries();
}
int file_manager_open_entry(uint32_t index) {
    platform_fs_entry_t entry;
    if (!entry_at(index, &entry)) return 0;
    selected_entry = (int32_t)index;
    if (entry.type == PLATFORM_FS_ENTRY_DIRECTORY) {
        char next[FM_PATH_CAP];
        uint32_t p = 0;
        if (text_equal(current_path, "/")) next[p++] = '/';
        else {
            while (current_path[p] && p < sizeof(next) - 2) { next[p] = current_path[p]; ++p; }
            if (p && next[p - 1] != '/') next[p++] = '/';
        }
        for (uint32_t i = 0; entry.name[i] && p < sizeof(next) - 1; ++i) next[p++] = entry.name[i];
        next[p] = '\0';
        file_manager_navigate(next);
        klog(LOG_INFO, "File manager opened directory entry");
        return 1;
    }

    uint64_t got = platform_storage_read(entry.handle, 0, sizeof(preview_text) - 1, (uint8_t*)preview_text);
    preview_text[got < sizeof(preview_text) ? got : sizeof(preview_text) - 1] = '\0';
    text_copy(preview_name, entry.name, sizeof(preview_name));
    if (!preview_win) preview_win = compositor_create_window("PHOXTRA FILE PREVIEW", 280, 170, 450, 190, 0x00020617);
    if (preview_win) {
        preview_win->active = 1;
        preview_win->minimized = 0;
        compositor_raise_window(preview_win);
    }
    klog(LOG_INFO, "Opened platform-storage file preview");
    return 1;
}
void file_manager_create_folder(const char* name) {
    if (!name || !name[0]) return;
    if (!current_directory) current_directory = platform_storage_lookup(current_path);
    if (platform_storage_create_directory(current_directory, name) == PLATFORM_OK) {
        redraw_entries();
        klog(LOG_INFO, "Created RAM-backed folder");
    } else klog(LOG_WARNING, "Folder creation failed (invalid or duplicate name)");
}
void file_manager_create_file(const char* name, const char* content) {
    if (!name || !name[0]) return;
    if (!current_directory) current_directory = platform_storage_lookup(current_path);
    if (platform_storage_create_text_file(current_directory, name, content ? content : "") == PLATFORM_OK) {
        redraw_entries();
        klog(LOG_INFO, "Created RAM-backed file");
    } else klog(LOG_WARNING, "File creation failed (invalid name, duplicate or allocation error)");
}
void file_manager_render(void) {
    redraw_entries();
    draw_preview();
}
