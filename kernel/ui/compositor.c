#include "compositor.h"
#include "font.h"
#include "theme.h"
#include "../drivers/framebuffer.h"
#include "../log/logger.h"
#include "phoxtra_logo.h"

#define MAX_WINDOWS 16
static window_t window_list[MAX_WINDOWS];
static uint32_t z_order[MAX_WINDOWS];
static uint32_t window_count = 0;

uint32_t compositor_window_count(void) { return window_count; }
window_t* compositor_window_at(uint32_t i) { return i < window_count ? &window_list[i] : (window_t*)0; }
window_t* compositor_window_at_point(uint32_t x, uint32_t y) {
    for (uint32_t i = window_count; i > 0; --i) {
        window_t* w = &window_list[z_order[i - 1]];
        if (w->active && !w->minimized && x >= w->x && y >= w->y && x < w->x + w->width && y < w->y + w->height) return w;
    }
    return (window_t*)0;
}

void compositor_init(void) {
    klog(LOG_INFO, "Initializing Window Compositor Engine (phoxtra-wm)...");
    window_count = 0;
    for (int i = 0; i < MAX_WINDOWS; i++) { window_list[i].active = 0; z_order[i] = (uint32_t)i; }
}

window_t* compositor_create_window(const char* title, uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t bg_color) {
    if (window_count >= MAX_WINDOWS) return 0;

    window_t* win = &window_list[window_count];
    win->id        = window_count + 1;
    win->x         = x;
    win->y         = y;
    win->width     = width;
    win->height    = height;
    win->bg_color  = bg_color;
    win->active    = 1;
    win->minimized = 0;
    win->maximized = 0;
    win->restore_x = x; win->restore_y = y; win->restore_width = width; win->restore_height = height;

    int pos = 0;
    while (title && title[pos] && pos < 63) {
        win->title[pos] = title[pos];
        pos++;
    }
    win->title[pos] = '\0';

    window_count++;
    return win;
}

void compositor_raise_window(window_t* window) {
    if (!window) return;
    uint32_t index = MAX_WINDOWS;
    for (uint32_t i = 0; i < window_count; ++i) {
        if (&window_list[z_order[i]] == window) { index = i; break; }
    }
    if (index >= window_count || index == window_count - 1) return;
    uint32_t id = z_order[index];
    for (uint32_t i = index; i + 1 < window_count; ++i) z_order[i] = z_order[i + 1];
    z_order[window_count - 1] = id;
}

void compositor_render_frame(void) {
    // 1. Draw Modern Glassmorphic Desktop Background Gradient
    theme_palette_t* palette = theme_get_current();
    uint32_t wallpaper_start = palette->desktop_bg_start;
    uint32_t wallpaper_end = palette->desktop_bg_end;
    theme_get_wallpaper_colors(&wallpaper_start, &wallpaper_end);
    fb_draw_gradient(0, 0, FB_WIDTH, FB_HEIGHT, wallpaper_start, wallpaper_end);

    // 2. Render Windows
    for (uint32_t i = 0; i < window_count; i++) {
        window_t* win = &window_list[z_order[i]];
        if (!win->active || win->minimized) continue;

        // Window Drop Shadow
        fb_fill_rect(win->x + 8, win->y + 8, win->width, win->height, 0x55000000);

        // Window Body
        fb_fill_rect(win->x, win->y, win->width, win->height, win->bg_color);
        fb_draw_rect(win->x, win->y, win->width, win->height, 0x00334155);

        // Window Title Bar
        fb_fill_rect(win->x, win->y, win->width, 32, 0x001E293B);
        fb_draw_rect(win->x, win->y, win->width, 32, 0x00475569);

        // Action Buttons (Close / Minimize / Maximize)
        fb_fill_rect(win->x + 12, win->y + 10, 12, 12, 0x00EF4444); // Red Close
        fb_fill_rect(win->x + 30, win->y + 10, 12, 12, 0x00F59E0B); // Yellow Min
        fb_fill_rect(win->x + 48, win->y + 10, 12, 12, 0x0010B981); // Green Max

        // Window Title Text
        font_draw_string_bold(win->title, win->x + 70, win->y + 8, 0x00F8FAFC, 0x00000000);
    }

    // 3. Render Modern Desktop Taskbar / Dock
    fb_fill_rect(0, FB_HEIGHT - 48, FB_WIDTH, 48, 0xCC0F172A);
    fb_draw_rect(0, FB_HEIGHT - 48, FB_WIDTH, 48, 0x00334155);

    // Taskbar Launcher Buttons & System Clock
    fb_fill_rect(8, FB_HEIGHT - 48, 140, 48, 0x000F172A);
    fb_draw_rect(8, FB_HEIGHT - 48, 140, 48, 0x0006B6D4);
    phoxtra_logo_draw(12, FB_HEIGHT - 48, 48);
    font_draw_string_bold("PHOXTRA", 64, FB_HEIGHT - 30, 0x0038BDF8, 0x00000000);

    font_draw_string("TERMINAL", 150, FB_HEIGHT - 32, 0x0094A3B8, 0x00000000);
    font_draw_string("SECURITY", 240, FB_HEIGHT - 32, 0x0094A3B8, 0x00000000);
    font_draw_string("NETWORK",  330, FB_HEIGHT - 32, 0x0094A3B8, 0x00000000);

    font_draw_string("PHOXTRA OS", FB_WIDTH - 120, FB_HEIGHT - 32, 0x00F8FAFC, 0x00000000);

    /* The window manager presents after drawing all desktop layers. */
}
