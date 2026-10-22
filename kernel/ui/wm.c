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
    klog(LOG_INFO, "Starting Phoxtra desktop at 1024x768");
}

static void wm_draw_desktop_icons(void) {
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
    }
    if (sec_win && sec_win->active && !sec_win->minimized) {
        font_draw_string("SYSTEM STATUS", sec_win->x + 16, sec_win->y + 50, 0x00FFD700, 0);
        font_draw_string("VFS workspace: temporary RAM storage", sec_win->x + 16, sec_win->y + 78, 0x0034D399, 0);
        font_draw_string("Input: PS/2 keyboard and mouse when detected", sec_win->x + 16, sec_win->y + 100, 0x0034D399, 0);
        font_draw_string("Persistent storage and application launching are not yet complete.", sec_win->x + 16, sec_win->y + 122, 0x0094A3B8, 0);
    }
}

static void wm_draw_pointer(void) {
    int32_t px = 0, py = 0;
    if (platform_input_pointer_position(&px, &py) != PLATFORM_OK) return;
    if (px < 0 || py < 0 || px >= FB_WIDTH || py >= FB_HEIGHT) return;
    platform_display_fill_rect((uint32_t)px, (uint32_t)py, 2, 15, 0x00FFFFFF);
    platform_display_fill_rect((uint32_t)px, (uint32_t)py, 8, 2, 0x00FFFFFF);
    platform_display_fill_rect((uint32_t)(px + 2 < FB_WIDTH ? px + 2 : px), (uint32_t)(py + 3 < FB_HEIGHT ? py + 3 : py), 2, 9, 0x00000000);
}

static void wm_render_desktop(void) {
    compositor_render_frame();
    wm_draw_desktop_icons();
    wm_draw_window_contents();
    if (fm_win && fm_win->active && !fm_win->minimized) file_manager_render();
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

static void wm_handle_desktop_icon(uint32_t x, uint32_t y) {
    if (x >= 130) return;
    file_manager_open_window("/home/user");
    fm_win = file_manager_get_window();
    wm_restore_window(fm_win);
    if (y >= 82 && y < 154) file_manager_navigate("/home/user/Documents");
    else if (y >= 154 && y < 226) file_manager_navigate("/home/user/Downloads");
    else if (y >= 226 && y < 298) file_manager_navigate("/home/user/Pictures");
    else if (y >= 298 && y < 370) file_manager_navigate("/home/user/Desktop");
    else file_manager_navigate("/");
}

static void wm_handle_window_button(window_t* win, uint32_t x, uint32_t y) {
    (void)y;
    uint32_t local_x = x - win->x;
    if (local_x < 28) {
        win->active = 0;
        if (focused_win == win) focused_win = 0;
        drag_win = 0;
        return;
    }
    if (local_x < 46) {
        win->minimized = 1;
        drag_win = 0;
        return;
    }
    if (local_x < 64) {
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
    drag_win = win;
    drag_dx = (int32_t)x - (int32_t)win->x;
    drag_dy = (int32_t)y - (int32_t)win->y;
}

static void wm_handle_click(uint32_t x, uint32_t y) {
    klog(LOG_INFO, "Desktop mouse click event received");
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
            klog(LOG_INFO, "File manager entry hit-test succeeded");
            uint64_t now = timer_get_ticks();
            file_manager_select_entry((uint32_t)index);
            selected_file_entry = index;
            /* Directories open on the first click; files retain double-click preview. */
            if (file_manager_entry_is_directory((uint32_t)index)) {
                (void)file_manager_open_entry((uint32_t)index);
                last_file_entry = -1;
            } else if (last_file_entry == index && now - last_file_click_tick <= 45) {
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
            if (event.buttons & PLATFORM_INPUT_LEFT_CLICKED)
                wm_handle_click((uint32_t)event.x, (uint32_t)event.y);
            if ((event.buttons & PLATFORM_INPUT_LEFT_PRESSED) && drag_win) {
                int32_t nx = event.x - drag_dx;
                int32_t ny = event.y - drag_dy;
                if (nx < 0) nx = 0;
                if (ny < 0) ny = 0;
                if (nx + (int32_t)drag_win->width > FB_WIDTH) nx = (int32_t)FB_WIDTH - (int32_t)drag_win->width;
                if (ny + 32 > FB_HEIGHT - 48) ny = (int32_t)FB_HEIGHT - 80;
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
