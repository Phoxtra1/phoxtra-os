#ifndef PHOXTRA_THEME_H
#define PHOXTRA_THEME_H

#include <stdint.h>

#define THEME_MIDNIGHT_GOLD 0
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

// Icon rendering engines (48x48 pixel icons)
void icon_draw_folder(uint32_t x, uint32_t y, const char* label);
void icon_draw_file(uint32_t x, uint32_t y, const char* label);
void icon_draw_drive(uint32_t x, uint32_t y, const char* label);
void icon_draw_phoxtra_badge(uint32_t x, uint32_t y);

#ifdef __cplusplus
}
#endif

#endif
