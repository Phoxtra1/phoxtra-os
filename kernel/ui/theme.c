#include "theme.h"
#include "font.h"
#include "../drivers/framebuffer.h"
#include "../log/logger.h"

static theme_palette_t themes[3] = {
    // 0: Phoxtra Blue (Default Brand Theme)
    {
        .desktop_bg_start = 0x000F172A,
        .desktop_bg_end   = 0x001E293B,
        .taskbar_bg       = 0xEE090D16,
        .window_bg        = 0x00020617,
        .titlebar_bg      = 0x00111827,
        .accent_primary   = 0x0038BDF8, // Royal Amber Gold
        .accent_secondary = 0x00007ACC, // Cyber Sapphire
        .folder_color     = 0x0038BDF8, // Phoxtra light blue (default)
        .file_color       = 0x00E2E8F0, // Soft Slate File
        .text_color       = 0x00F8FAFC
    },
    // 1: Cyber Sapphire
    {
        .desktop_bg_start = 0x000A192F,
        .desktop_bg_end   = 0x00172A45,
        .taskbar_bg       = 0xEE0A192F,
        .window_bg        = 0x00020C1B,
        .titlebar_bg      = 0x000F2A4A,
        .accent_primary   = 0x0064FFDA,
        .accent_secondary = 0x00007ACC,
        .folder_color     = 0x003B82F6,
        .file_color       = 0x00E2E8F0,
        .text_color       = 0x00CCD6F6
    },
    // 2: Emerald Dark
    {
        .desktop_bg_start = 0x00064E3B,
        .desktop_bg_end   = 0x00022C22,
        .taskbar_bg       = 0xEE022C22,
        .window_bg        = 0x00021A15,
        .titlebar_bg      = 0x00065F46,
        .accent_primary   = 0x0010B981,
        .accent_secondary = 0x00059669,
        .folder_color     = 0x0034D399,
        .file_color       = 0x00ECFDF5,
        .text_color       = 0x00F0FDF4
    }
};

static uint32_t active_theme_index = 0;
typedef struct desktop_wallpaper { const char* name; uint32_t start; uint32_t end; } desktop_wallpaper_t;
static const desktop_wallpaper_t desktop_wallpapers[] = {
    {"Blue Horizon", 0x0009162Du, 0x001D4E89u},
    {"Midnight",     0x00050916u, 0x001A2440u},
    {"Aurora",       0x0005212Du, 0x001C5B69u},
    {"Violet Dusk",  0x00151B36u, 0x003B3260u}
};
static uint32_t active_wallpaper_index = 0;

void theme_init(void) {
    active_theme_index = 0;
    active_wallpaper_index = 0;
    klog(LOG_INFO, "Phoxtra Theme System Initialized (Active: Midnight Gold)");
}

void theme_set(uint32_t theme_id) {
    if (theme_id > 2) theme_id = 0;
    active_theme_index = theme_id;
    klog(LOG_INFO, "Phoxtra Desktop Theme Changed");
}

theme_palette_t* theme_get_current(void) {
    return &themes[active_theme_index];
}
uint32_t theme_get_current_index(void) { return active_theme_index; }

uint32_t theme_get_wallpaper_count(void) {
    return (uint32_t)(sizeof(desktop_wallpapers) / sizeof(desktop_wallpapers[0]));
}
uint32_t theme_get_wallpaper_index(void) { return active_wallpaper_index; }
const char* theme_get_wallpaper_name(uint32_t id) {
    if (id >= theme_get_wallpaper_count()) id = 0;
    return desktop_wallpapers[id].name;
}
void theme_set_wallpaper(uint32_t id) {
    if (id >= theme_get_wallpaper_count()) return;
    active_wallpaper_index = id;
    klog(LOG_INFO, "Desktop wallpaper changed");
}
void theme_get_wallpaper_colors(uint32_t* start, uint32_t* end) {
    const desktop_wallpaper_t* w = &desktop_wallpapers[active_wallpaper_index];
    if (start) *start = w->start;
    if (end) *end = w->end;
}

void icon_draw_folder(uint32_t x, uint32_t y, const char* label) {
    theme_palette_t* pal = theme_get_current();
    uint32_t color = pal->folder_color;

    /* All folders share the selected theme colour. */
    fb_fill_rect(x + 4, y, 20, 8, color);
    fb_fill_rect(x, y + 6, 44, 34, color);
    fb_draw_rect(x, y + 6, 44, 34, pal->accent_secondary);
    fb_fill_rect(x + 2, y + 10, 40, 4, 0x00BAE6FD);

    if (label)
        font_draw_string(label, x - 4, y + 44, pal->text_color, 0x00000000);
}

void icon_draw_file(uint32_t x, uint32_t y, const char* label) {
    theme_palette_t* pal = theme_get_current();

    // Document Body (32x40 px)
    fb_fill_rect(x + 6, y, 32, 40, pal->file_color);
    fb_draw_rect(x + 6, y, 32, 40, 0x0094A3B8);

    // Folded Corner (Top Right)
    fb_fill_rect(x + 30, y, 8, 8, pal->desktop_bg_start);

    // Document Lines
    fb_fill_rect(x + 12, y + 10, 20, 2, 0x0064748B);
    fb_fill_rect(x + 12, y + 16, 20, 2, 0x0064748B);
    fb_fill_rect(x + 12, y + 22, 14, 2, 0x0064748B);

    // Label Text
    if (label) {
        font_draw_string(label, x - 4, y + 44, pal->text_color, 0x00000000);
    }
}

void icon_draw_drive(uint32_t x, uint32_t y, const char* label) {
    theme_palette_t* pal = theme_get_current();

    // Drive Body
    fb_fill_rect(x, y + 8, 46, 28, 0x00475569);
    fb_draw_rect(x, y + 8, 46, 28, pal->accent_primary);

    // Status LED & Activity Lines
    fb_fill_rect(x + 36, y + 14, 6, 6, 0x0010B981); // Green Power LED
    fb_fill_rect(x + 6, y + 18, 24, 4, 0x00334155);

    if (label) {
        font_draw_string(label, x - 4, y + 44, pal->text_color, 0x00000000);
    }
}

void icon_draw_phoxtra_badge(uint32_t x, uint32_t y) {
    // Render Cyan Hexagonal 'P' Emblem Logo (#06B6D4 / #38BDF8)
    uint32_t cyan_color = 0x0006B6D4;
    uint32_t cyan_light = 0x0038BDF8;

    // Hexagonal Outer Border Geometry
    fb_fill_rect(x + 8, y, 16, 4, cyan_color);         // Top edge
    fb_fill_rect(x + 4, y + 4, 4, 24, cyan_color);      // Left edge
    fb_fill_rect(x + 24, y + 4, 4, 24, cyan_color);     // Right edge
    fb_fill_rect(x + 8, y + 28, 16, 4, cyan_color);     // Bottom edge

    // Inner Hexagonal 'P' Loop
    fb_fill_rect(x + 10, y + 6, 10, 4, cyan_light);
    fb_fill_rect(x + 10, y + 6, 4, 18, cyan_light);    // Vertical stem
    fb_fill_rect(x + 14, y + 12, 6, 4, cyan_light);    // Middle P loop
    fb_fill_rect(x + 18, y + 8, 4, 6, cyan_light);     // Right P curve
}
