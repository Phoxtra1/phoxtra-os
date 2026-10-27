#include "wm.h"
#include "compositor.h"
#include "font.h"
#include "theme.h"
#include "file_manager.h"
#include "../drivers/framebuffer.h"
#include "../installer/installer.h"
#include "../log/logger.h"
#include "../src/mouse.h"
#include "keyboard.h"
#include "timer.h"
#include "../services/platform.h"

static window_t* term_win = 0;
static window_t* sec_win = 0;
static window_t* mon_win = 0;
static window_t* fm_win = 0;
static window_t* focused_win = 0;
static window_t* drag_win = 0;
static int32_t drag_dx = 0, drag_dy = 0;
static int32_t selected_file_entry = -1;
static int32_t last_file_entry = -1;
static uint64_t last_file_click_tick = 0;
static int32_t selected_desktop_icon = -1;
static int32_t last_desktop_icon = -1;
static uint64_t last_desktop_click_tick = 0;
static uint8_t context_menu_open = 0;
static uint32_t context_menu_x = 0, context_menu_y = 0;
#define DESKTOP_MENU_WIDTH 304u
#define DESKTOP_MENU_HEIGHT 310u


static void wm_create_new_folder(void);
static void wm_create_new_file(void);
static void wm_draw_context_menu(void);
static int wm_handle_context_menu_click(uint32_t x, uint32_t y);
static void wm_handle_right_click(uint32_t x, uint32_t y);

void wm_init(void) {
    klog(LOG_INFO, "Initializing Phoxtra desktop window manager");
    theme_init();
    file_manager_init();
    installer_init();

    /* Keep the left icon rail clear and prevent the top windows from overlapping. */
    term_win = compositor_create_window("PHOXTRA TERMINAL", 170, 54, 400, 300, 0x00020617);
    file_manager_open_window("/home/user");
    fm_win = file_manager_get_window();
    sec_win = compositor_create_window("SECURITY AND SYSTEM STATUS", 170, 374, 832, 310, 0x00020617);
    focused_win = term_win;
    (void)mon_win;
}

void wm_start_desktop(void) {
    klog(LOG_INFO, "Starting Phoxtra desktop splash animation");
    const uint64_t start_ticks = timer_get_ticks();
    uint64_t last_frame = ~(uint64_t)0;
    const uint64_t duration_ticks = 260u; /* 2.6 seconds at the configured 100 Hz timer. */

    while (1) {
        uint64_t now = timer_get_ticks();
        uint64_t elapsed = now - start_ticks;
        if (elapsed >= duration_ticks) break;
        uint64_t frame = elapsed / 4u;
        if (frame != last_frame) {
            uint32_t bg_start = 0x0009162Du, bg_end = 0x001D4E89u;
            theme_get_wallpaper_colors(&bg_start, &bg_end);
            fb_draw_gradient(0, 0, FB_WIDTH, FB_HEIGHT, bg_start, bg_end);

            /* A restrained pulse keeps the brand mark monochrome and crisp. */
            uint32_t size = 46u + (uint32_t)(frame % 7u);
            uint32_t logo_x = (FB_WIDTH - size) / 2u;
            uint32_t logo_y = 300u - size / 2u;
            compositor_draw_phoxtra_mark(logo_x, logo_y, size, 0x00DCEBFFu);

            font_draw_string_bold("PHOXTRA", FB_WIDTH / 2u - (7u * FONT_WIDTH) / 2u,
                                  354u, 0x00F8FAFCu, 0);
            font_draw_string("Preparing your desktop", FB_WIDTH / 2u - 100u,
                             393u, 0x00CBD5E1u, 0);

            fb_fill_rect(FB_WIDTH / 2u - 150u, 435u, 300u, 4u, 0x0030445Du);
            uint32_t progress = (uint32_t)((elapsed * 300u) / duration_ticks);
            if (progress > 300u) progress = 300u;
            fb_fill_rect(FB_WIDTH / 2u - 150u, 435u, progress, 4u, 0x0038BDF8u);
            platform_display_present();
            last_frame = frame;
        }
        __asm__ volatile ("sti; hlt" ::: "memory");
    }
}

static void wm_draw_desktop_icons(void) {
    if (selected_desktop_icon == 0) fb_fill_rect(8, 16, 72, 66, 0x0030445D);
    if (selected_desktop_icon == 1) fb_fill_rect(8, 88, 72, 66, 0x0030445D);
    if (selected_desktop_icon == 2) fb_fill_rect(8, 160, 72, 66, 0x0030445D);
    if (selected_desktop_icon == 3) fb_fill_rect(8, 232, 72, 66, 0x0030445D);
    if (selected_desktop_icon == 4) fb_fill_rect(8, 304, 72, 66, 0x0030445D);
    icon_draw_drive(18, 20, "System");
    icon_draw_folder(18, 92, "Documents");
    icon_draw_folder(18, 164, "Downloads");
    icon_draw_folder(18, 236, "Pictures");
    icon_draw_folder(18, 308, "Desktop");
}

static void wm_draw_window_contents(void) {
    if (term_win && term_win->active && !term_win->minimized) {
        font_draw_string("AKWAABA - PHOXTRA OS", term_win->x + 16, term_win->y + 50, 0x00FFD700, 0);
        font_draw_string("Terminal shell is available after leaving the desktop.", term_win->x + 16, term_win->y + 76, 0x00F8FAFC, 0);
        font_draw_string("Esc: return to shell", term_win->x + 16, term_win->y + 102, 0x0094A3B8, 0);
        font_draw_string("F: show file manager", term_win->x + 16, term_win->y + 124, 0x0094A3B8, 0);
        font_draw_string("1: Light Blue  2: Blue  3: Emerald", term_win->x + 16, term_win->y + 148, 0x0094A3B8, 0);
    }
    if (sec_win && sec_win->active && !sec_win->minimized) {
        font_draw_string("SYSTEM STATUS", sec_win->x + 16, sec_win->y + 50, 0x00FFD700, 0);
        font_draw_string("VFS workspace: temporary RAM storage", sec_win->x + 16, sec_win->y + 78, 0x0034D399, 0);
        font_draw_string("Input: PS/2 keyboard and mouse when detected", sec_win->x + 16, sec_win->y + 100, 0x0034D399, 0);
        font_draw_string("Persistent storage and application launching are not yet complete.", sec_win->x + 16, sec_win->y + 122, 0x0094A3B8, 0);
    }
}

static void wm_draw_pointer(void) {
    static const char* const arrow[18] = {
        "X...........", "XX..........", "XWX.........", "XWWX........",
        "XWWWX.......", "XWWWWX......", "XWWWWWX.....", "XWWWWWWX....",
        "XWWWWWWWX...", "XWWWWWWWWX..", "XWWWWXXXXX..", "XXWWX.......",
        ".XWWX.......", ".XWWX.......", "..XWWX......", "..XWWX......",
        "..XX........", "..X........."
    };
    int32_t px = 0, py = 0;
    if (platform_input_pointer_position(&px, &py) != PLATFORM_OK) return;
    if (px < 0 || py < 0 || px >= FB_WIDTH || py >= FB_HEIGHT) return;

    for (uint32_t row = 0; row < 18u; ++row) {
        for (uint32_t col = 0; col < 12u; ++col) {
            char mark = arrow[row][col];
            if (mark == '.') continue;
            uint32_t x = (uint32_t)px + col;
            uint32_t y = (uint32_t)py + row;
            if (x >= FB_WIDTH || y >= FB_HEIGHT) continue;
            fb_put_pixel(x, y, mark == 'W' ? 0x00FFFFFFu : 0x00030A16u);
        }
    }
}


static void wm_draw_context_menu(void) {
    if (!context_menu_open) return;

    uint32_t menu_w = 176;
    uint32_t menu_h = 76;
    uint32_t x = context_menu_x;
    uint32_t y = context_menu_y;

    if (x + menu_w > FB_WIDTH) x = FB_WIDTH - menu_w;
    if (y + menu_h > FB_HEIGHT - 48) y = FB_HEIGHT - 48 - menu_h;

    platform_display_fill_rect(x, y, menu_w, menu_h, 0x001E293B);
    platform_display_fill_rect(x, y, menu_w, 2, 0x0038BDF8);
    font_draw_string("New Folder", x + 12, y + 12, 0x00F8FAFC, 0x001E293B);
    font_draw_string("New File",   x + 12, y + 34, 0x00F8FAFC, 0x001E293B);
    font_draw_string("Close Menu", x + 12, y + 56, 0x0094A3B8, 0x001E293B);
}

static int wm_handle_context_menu_click(uint32_t x, uint32_t y) {
    if (!context_menu_open) return 0;

    uint32_t menu_x = context_menu_x;
    uint32_t menu_y = context_menu_y;
    const uint32_t menu_w = 176;
    const uint32_t menu_h = 76;

    if (menu_x + menu_w > FB_WIDTH) menu_x = FB_WIDTH - menu_w;
    if (menu_y + menu_h > FB_HEIGHT - 48)
        menu_y = FB_HEIGHT - 48 - menu_h;

    if (x >= menu_x && x < menu_x + menu_w &&
        y >= menu_y && y < menu_y + menu_h) {
        uint32_t item = (y - menu_y) / 24;
        context_menu_open = 0;

        if (item == 0) wm_create_new_folder();
        else if (item == 1) wm_create_new_file();
        return 1;
    }

    context_menu_open = 0;
    return 0;
}

static void wm_handle_right_click(uint32_t x, uint32_t y) {
    if (y >= FB_HEIGHT - 48) {
        context_menu_open = 0;
        return;
    }

    context_menu_x = x;
    context_menu_y = y;
    context_menu_open = 1;
}

static void wm_render_desktop(void) {
    compositor_render_frame();
    wm_draw_desktop_icons();
    wm_draw_window_contents();
    if (fm_win && fm_win->active && !fm_win->minimized) file_manager_render();
    wm_draw_context_menu();
    wm_draw_pointer();
    platform_display_present();
}

static void wm_restore_window(window_t* win) {
    if (!win) return;
    win->active = 1;
    win->minimized = 0;
    compositor_raise_window(win);
    focused_win = win;
}

static void wm_handle_dock_click(uint32_t x) {
    if (x < 142) {
        file_manager_open_window("/home/user");
        fm_win = file_manager_get_window();
        wm_restore_window(fm_win);
    } else if (x < 225) {
        wm_restore_window(term_win);
    } else if (x < 315) {
        wm_restore_window(sec_win);
    } else if (x < 430) {
        file_manager_open_window("/home/user");
        fm_win = file_manager_get_window();
        wm_restore_window(fm_win);
    }
}

static int wm_desktop_icon_at(uint32_t x, uint32_t y) {
    if (x < 8 || x >= 80 || y < 16 || y >= 370) return -1;
    uint32_t row = (y - 16u) / 72u;
    if (row >= 5u || y >= 16u + row * 72u + 66u) return -1;
    return (int)row;
}

static void wm_open_desktop_icon(int32_t index) {
    const char* path = "/";
    if (index == 1) path = "/home/user/Documents";
    else if (index == 2) path = "/home/user/Downloads";
    else if (index == 3) path = "/home/user/Pictures";
    else if (index == 4) path = "/home/user/Desktop";
    file_manager_open_window("/home/user");
    fm_win = file_manager_get_window();
    wm_restore_window(fm_win);
    if (index == 0) file_manager_navigate("/");
    else file_manager_navigate(path);
}

static void wm_handle_desktop_icon(uint32_t x, uint32_t y) {
    int32_t index = wm_desktop_icon_at(x, y);
    if (index < 0) { selected_desktop_icon = -1; return; }
    uint64_t now = timer_get_ticks();
    selected_desktop_icon = index;
    if (last_desktop_icon == index && now - last_desktop_click_tick <= 45u) {
        last_desktop_icon = -1;
        selected_desktop_icon = -1;
        wm_open_desktop_icon(index);
    } else {
        last_desktop_icon = index;
    }
    last_desktop_click_tick = now;
}

static void wm_handle_window_button(window_t* win, uint32_t x, uint32_t y) {
    (void)y;
    uint32_t local_x = x - win->x;
    if (local_x >= 12 && local_x < 28) {
        win->active = 0;
        if (focused_win == win) focused_win = 0;
        drag_win = 0;
        return;
    }
    if (local_x >= 30 && local_x < 46) {
        win->minimized = 1;
        drag_win = 0;
        return;
    }
    if (local_x >= 48 && local_x < 64) {
        if (!win->maximized) {
            win->restore_x = win->x; win->restore_y = win->y;
            win->restore_width = win->width; win->restore_height = win->height;
            win->x = 0; win->y = 0; win->width = FB_WIDTH; win->height = FB_HEIGHT - 48;
            win->maximized = 1;
        } else {
            win->x = win->restore_x; win->y = win->restore_y;
            win->width = win->restore_width; win->height = win->restore_height;
            win->maximized = 0;
        }
        drag_win = 0;
        return;
    }
    if (win->maximized) {
        drag_win = 0;
        return;
    }
    drag_win = win;
    drag_dx = (int32_t)x - (int32_t)win->x;
    drag_dy = (int32_t)y - (int32_t)win->y;
}

static void wm_handle_click(uint32_t x, uint32_t y) {
    if (context_menu_open && wm_handle_context_menu_click(x, y)) return;
    if (y >= FB_HEIGHT - 48) {
        wm_handle_dock_click(x);
        return;
    }

    window_t* win = compositor_window_at_point(x, y);
    if (!win) {
        wm_handle_desktop_icon(x, y);
        return;
    }
    compositor_raise_window(win);
    focused_win = win;

    if (y < win->y + 32) {
        wm_handle_window_button(win, x, y);
        return;
    }

    if (win == fm_win) {
        int index = file_manager_hit_test(x, y);
        if (index >= 0) {
            uint64_t now = timer_get_ticks();
            file_manager_select_entry((uint32_t)index);
            selected_file_entry = index;
            /* Single click selects; a second click on the same item opens it. */
            if (last_file_entry == index && now - last_file_click_tick <= 45u) {
                (void)file_manager_open_entry((uint32_t)index);
                last_file_entry = -1;
            } else {
                last_file_entry = index;
            }
            last_file_click_tick = now;
        }
    }
}

static void wm_create_new_folder(void) {
    file_manager_create_folder("New Folder");
}
static void wm_create_new_file(void) {
    file_manager_create_file("New File.txt", "Welcome to Phoxtra OS.\nThis file is stored in temporary RAM.\n");
}

static void wm_handle_key(char key) {
    if (key == 27) return;
    if (key == 'f' || key == 'F') {
        file_manager_open_window("/home/user");
        fm_win = file_manager_get_window();
        wm_restore_window(fm_win);
        return;
    }
    if (key == 't' || key == 'T') { wm_restore_window(term_win); return; }
    if (key == 's' || key == 'S') { wm_restore_window(sec_win); return; }

    if (key >= '1' && key <= '3') {
        theme_set((uint32_t)(key - '1'));
        return;
    }

    if (focused_win != fm_win || !fm_win || !fm_win->active) return;
    if (key == '\b') {
        file_manager_go_parent();
        selected_file_entry = -1;
        return;
    }
    if (key == '\n') {
        if (selected_file_entry >= 0) {
            (void)file_manager_open_entry((uint32_t)selected_file_entry);
            selected_file_entry = -1;
        }
        return;
    }
    if (key == '[') { file_manager_scroll(-1); return; }
    if (key == ']') { file_manager_scroll(1); return; }
    if (key == 'n' || key == 'N') wm_create_new_folder();
    else if (key == 'c' || key == 'C') wm_create_new_file();
}

void wm_desktop_event_loop(void) {
    klog(LOG_INFO, "Desktop event loop active");
    wm_render_desktop();
    uint64_t last_redraw = timer_get_ticks();
    for (;;) {
        platform_input_event_t event;
        int event_result = platform_input_poll(&event);
        if (event_result > 0 && event.type == PLATFORM_INPUT_POINTER) {
            if (event.buttons & PLATFORM_INPUT_RIGHT_CLICKED)
                wm_handle_right_click((uint32_t)event.x, (uint32_t)event.y);
            if (event.buttons & PLATFORM_INPUT_LEFT_CLICKED)
                wm_handle_click((uint32_t)event.x, (uint32_t)event.y);
            if ((event.buttons & PLATFORM_INPUT_LEFT_PRESSED) && drag_win) {
                int32_t max_x = (int32_t)FB_WIDTH - (int32_t)drag_win->width;
                int32_t max_y = (int32_t)FB_HEIGHT - 48 - (int32_t)drag_win->height;
                if (max_x < 0) max_x = 0;
                if (max_y < 0) max_y = 0;
                int32_t nx = event.x - drag_dx;
                int32_t ny = event.y - drag_dy;
                if (nx < 0) nx = 0;
                if (ny < 0) ny = 0;
                if (nx > max_x) nx = max_x;
                if (ny > max_y) ny = max_y;
                drag_win->x = (uint32_t)nx;
                drag_win->y = (uint32_t)ny;
            } else if (!(event.buttons & PLATFORM_INPUT_LEFT_PRESSED)) {
                drag_win = 0;
            }
            last_redraw = 0;
        }

        if (event_result > 0 && event.type == PLATFORM_INPUT_KEY) {
            char key = (char)event.key;
            if (key == 27) break;
            wm_handle_key(key);
            last_redraw = 0;
        }

        uint64_t now = timer_get_ticks();
        if (last_redraw == 0 || now - last_redraw >= 2) {
            wm_render_desktop();
            last_redraw = now;
        }
        __asm__ volatile ("sti; hlt" ::: "memory");
    }
    klog(LOG_INFO, "Desktop event loop returned to shell");
}

uint32_t wm_get_window_count(void) {
    return compositor_window_count();
}
