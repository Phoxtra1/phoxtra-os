#include "compositor.h"
#include "font.h"
#include "theme.h"
#include "../drivers/framebuffer.h"
#include "../log/logger.h"
#include "phoxtra_logo.h"

#define MAX_WINDOWS 16
static window_t window_list[MAX_WINDOWS];
static uint32_t window_count = 0;

void compositor_init(void) {
    klog(LOG_INFO, "Initializing Window Compositor Engine (phoxtra-wm)...");
    window_count = 0;
    for (int i = 0; i < MAX_WINDOWS; i++) window_list[i].active = 0;
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

    int pos = 0;
    while (title && title[pos] && pos < 63) {
        win->title[pos] = title[pos];
        pos++;
    }
    win->title[pos] = '\0';

    window_count++;
    return win;
}

void compositor_render_frame(void) {
    // 1. Draw Modern Glassmorphic Desktop Background Gradient
    fb_draw_gradient(0, 0, FB_WIDTH, FB_HEIGHT, 0x000F172A, 0x001E293B);

    // 2. Render Windows
    for (uint32_t i = 0; i < window_count; i++) {
        window_t* win = &window_list[i];
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
        font_draw_string(win->title, win->x + 70, win->y + 8, 0x00F8FAFC, 0x00000000);
    }

    // 3. Render Modern Desktop Taskbar / Dock
    fb_fill_rect(0, FB_HEIGHT - 48, FB_WIDTH, 48, 0xCC0F172A);
    fb_draw_rect(0, FB_HEIGHT - 48, FB_WIDTH, 48, 0x00334155);

    // Taskbar Launcher Buttons & System Clock
    fb_fill_rect(12, FB_HEIGHT - 40, 130, 32, 0x000F172A);
    fb_draw_rect(12, FB_HEIGHT - 40, 130, 32, 0x0006B6D4);
    icon_draw_phoxtra_badge(16, FB_HEIGHT - 40);
    font_draw_string("PHOXTRA", 54, FB_HEIGHT - 30, 0x0038BDF8, 0x00000000);

    font_draw_string("TERMINAL", 150, FB_HEIGHT - 32, 0x0094A3B8, 0x00000000);
    font_draw_string("SECURITY", 240, FB_HEIGHT - 32, 0x0094A3B8, 0x00000000);
    font_draw_string("NETWORK",  330, FB_HEIGHT - 32, 0x0094A3B8, 0x00000000);

    font_draw_string("15:30 UTC", FB_WIDTH - 110, FB_HEIGHT - 32, 0x00F8FAFC, 0x00000000);

    // 4. Swap Framebuffers
    fb_swap_buffers();
}
