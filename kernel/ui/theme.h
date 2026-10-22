#ifndef PHOXTRA_THEME_H
#define PHOXTRA_THEME_H

#include <stdint.h>

#define THEME_PHOXTRA_BLUE 0
#define THEME_MIDNIGHT_GOLD THEME_PHOXTRA_BLUE
#define THEME_CYBER_SAPPHIRE 1
#define THEME_EMERALD_DARK 2

typedef struct theme_palette {
    uint32_t desktop_bg_start;
    uint32_t desktop_bg_end;
    uint32_t taskbar_bg;
    uint32_t window_bg;
    uint32_t titlebar_bg;
    uint32_t accent_primary;
    uint32_t accent_secondary;
    uint32_t folder_color;
    uint32_t file_color;
    uint32_t text_color;
} theme_palette_t;

#ifdef __cplusplus
extern "C" {
#endif

void theme_init(void);
void theme_set(uint32_t theme_id);
theme_palette_t* theme_get_current(void);
uint32_t theme_get_current_index(void);
void theme_set_wallpaper(uint32_t wallpaper_id);
uint32_t theme_get_wallpaper_index(void);
uint32_t theme_get_wallpaper_count(void);
const char* theme_get_wallpaper_name(uint32_t wallpaper_id);
void theme_get_wallpaper_colors(uint32_t* start_color, uint32_t* end_color);

// Icon rendering engines (48x48 pixel icons)
void icon_draw_folder(uint32_t x, uint32_t y, const char* label);
void icon_draw_file(uint32_t x, uint32_t y, const char* label);
void icon_draw_drive(uint32_t x, uint32_t y, const char* label);
void icon_draw_phoxtra_badge(uint32_t x, uint32_t y);

#ifdef __cplusplus
}
#endif

#endif
