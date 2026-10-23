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
#include "../net/netdev.h"

static window_t* term_win = 0;
static window_t* sec_win = 0;
static window_t* mon_win = 0;
static window_t* fm_win = 0;
static window_t* app_win = 0;
static window_t* focused_win = 0;
static uint8_t app_view_detail = 0;
static uint32_t app_selected_index = 0;
static uint32_t app_page = 0;
#define APP_CENTER_PAGE_SIZE 10u
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
    sec_win = compositor_create_window("SYSTEM STATUS", 170, 374, 832, 310, 0x00020617);
    app_win = compositor_create_window("APPLICATION CENTER", 150, 64, 800, 610, 0x00020617);
    if (app_win) app_win->active = 0;
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
        font_draw_string("A: open Application Center", term_win->x + 16, term_win->y + 146, 0x0094A3B8, 0);
    }
    if (sec_win && sec_win->active && !sec_win->minimized) {
        font_draw_string("SYSTEM STATUS", sec_win->x + 16, sec_win->y + 50, 0x00FFD700, 0);
        font_draw_string("Storage: temporary RAM-backed VFS", sec_win->x + 16, sec_win->y + 78, 0x0034D399, 0);
        font_draw_string("Input: detected keyboard and PS/2 pointer", sec_win->x + 16, sec_win->y + 100, 0x0034D399, 0);
        font_draw_string("Press A for Phoxtra and Amineva application entries.", sec_win->x + 16, sec_win->y + 122, 0x0094A3B8, 0);
    }
}

static void wm_open_application_center(uint32_t selected_index, uint8_t detail) {
    if (!app_win) return;
    app_view_detail = detail;
    app_selected_index = selected_index;
    if (!detail) app_page = 0;
    klog(LOG_INFO, detail ? "Application Center opened application details" : "Application Center opened");
    if (detail) {
        const platform_app_info_t* selected = 0;
        if (platform_app_get(selected_index, &selected) == PLATFORM_OK && selected &&
            selected->id && selected->id[0] == 'a' && selected->id[1] == 'm' &&
            selected->id[2] == 'i' && selected->id[3] == 'n' &&
            selected->id[4] == 'e' && selected->id[5] == 'v' &&
            selected->id[6] == 'a' && selected->id[7] == '-' &&
            selected->id[8] == 'w') klog(LOG_INFO, "Amineva Workspace entry opened");
    }
    app_win->active = 1;
    app_win->minimized = 0;
    compositor_raise_window(app_win);
    focused_win = app_win;
}

static void wm_draw_application_center(void) {
    if (!app_win || !app_win->active || app_win->minimized) return;
    theme_palette_t* palette = theme_get_current();
    if (app_view_detail) {
        const platform_app_info_t* app = 0;
        if (platform_app_get(app_selected_index, &app) != PLATFORM_OK || !app) {
            font_draw_string("Application entry is unavailable.", app_win->x + 24, app_win->y + 70, palette->text_color, 0);
            font_draw_string("Press B to return to the catalogue.", app_win->x + 24, app_win->y + 104, palette->text_color, 0);
            return;
        }
        font_draw_string(app->name, app_win->x + 24, app_win->y + 58, palette->accent_primary, 0);
        font_draw_string(app->publisher, app_win->x + 24, app_win->y + 84, palette->text_color, 0);
        font_draw_string(app->category, app_win->x + 24, app_win->y + 110, palette->text_color, 0);
        font_draw_string(app->application_type, app_win->x + 24, app_win->y + 136, palette->text_color, 0);
        font_draw_string(app->description, app_win->x + 24, app_win->y + 164, palette->text_color, 0);
        font_draw_string(app->launch_destination ? app->launch_destination : "Launch destination: not configured",
                         app_win->x + 24, app_win->y + 192, palette->text_color, 0);
        if (app->state == PLATFORM_APP_AVAILABLE) {
            font_draw_string("STATUS: AVAILABLE IN THIS BUILD", app_win->x + 24, app_win->y + 226, 0x0034D399, 0);
            font_draw_string("This native panel has an internal route.", app_win->x + 24, app_win->y + 252, palette->text_color, 0);
        } else {
            font_draw_string("STATUS: INTEGRATION NOT CONNECTED IN THIS BUILD", app_win->x + 24, app_win->y + 226, 0x00F59E0B, 0);
            font_draw_string("Catalogue entry only; no application launch is claimed.", app_win->x + 24, app_win->y + 252, palette->text_color, 0);
        }
        if (app->id && app->id[0] == 'a' && app->id[1] == 'm' && app->id[2] == 'i' &&
            app->id[3] == 'n' && app->id[4] == 'e' && app->id[5] == 'v' &&
            app->id[6] == 'a' && app->id[7] == '-' && app->id[8] == 'w') {
            font_draw_string("PRODUCTIVITY SUITE", app_win->x + 24, app_win->y + 302, palette->accent_primary, 0);
            font_draw_string("Docs | Sheets | Slides | Forms | Drive", app_win->x + 24, app_win->y + 332, palette->text_color, 0);
            font_draw_string("One integrated Workspace entry, not five unrelated products.", app_win->x + 24, app_win->y + 362, palette->text_color, 0);
            font_draw_string("Sign-in, shared files, launch hand-off and offline mode are unverified.", app_win->x + 24, app_win->y + 392, palette->text_color, 0);
        }
        font_draw_string("B: back to applications", app_win->x + 24, app_win->y + 520, palette->accent_secondary, 0);
        return;
    }

    font_draw_string("Application Center - publisher identities stay separate.", app_win->x + 24, app_win->y + 48, palette->text_color, 0);
    font_draw_string("Phoxtra apps | Amineva Global | independent services | third-party marketplace", app_win->x + 24, app_win->y + 70, palette->text_color, 0);
    uint32_t total = platform_app_count();
    uint32_t start = app_page * APP_CENTER_PAGE_SIZE;
    uint32_t stop = start + APP_CENTER_PAGE_SIZE;
    if (stop > total) stop = total;
    for (uint32_t i = start; i < stop; ++i) {
        const platform_app_info_t* app = 0;
        if (platform_app_get(i, &app) != PLATFORM_OK || !app) continue;
        uint32_t row_y = app_win->y + 94u + (i - start) * 38u;
        uint32_t row_color = (i == app_selected_index) ? palette->titlebar_bg : palette->window_bg;
        platform_display_fill_rect(app_win->x + 18u, row_y, app_win->width - 36u, 34u, row_color);
        font_draw_string(app->name, app_win->x + 28u, row_y + 5u, palette->text_color, 0);
        font_draw_string(app->publisher, app_win->x + 250u, row_y + 5u, palette->text_color, 0);
        font_draw_string(app->state == PLATFORM_APP_AVAILABLE ? "READY" : "PLANNED",
                         app_win->x + 670u, row_y + 5u,
                         app->state == PLATFORM_APP_AVAILABLE ? 0x0034D399 : 0x00F59E0B, 0);
    }
    font_draw_string(app_page == 0 ? "Page 1/2" : "Page 2/2", app_win->x + 24u, app_win->y + 500u, palette->accent_secondary, 0);
    font_draw_string("[: previous page  ]: next page  W: Workspace  Q: close", app_win->x + 24u, app_win->y + 526u, palette->accent_secondary, 0);
}

static void wm_draw_pointer(void) {
    /* Draw one crisp monochrome arrow into the backbuffer. The desktop is
       fully repainted before this function runs, so no previous cursor pixels
       are carried into the next frame. */
    static const char* const arrow[18] = {
        "X...........", "XX..........", "XW..........", "XWW.........",
        "XWWW........", "XWWWW.......", "XWWWWW......", "XWWWWWW.....",
        "XWWWWWWW....", "XWWWWWWWW...", "XWWWWXXXXX...", "XXWWX.......",
        ".XWWX.......", ".XWWX.......", "..XWWX......", "..XWWX......",
        "..XX........", "..X........."
    };
    int32_t px = 0, py = 0;
    if (platform_input_pointer_position(&px, &py) != PLATFORM_OK) return;
    if (px < 0 || py < 0 || px >= (int32_t)FB_WIDTH || py >= (int32_t)FB_HEIGHT) return;

    for (uint32_t row = 0; row < 18u; ++row) {
        for (uint32_t col = 0; col < 12u; ++col) {
            char mark = arrow[row][col];
            if (mark == '.') continue;
            uint32_t x = (uint32_t)px + col;
            uint32_t y = (uint32_t)py + row;
            if (x >= FB_WIDTH || y >= FB_HEIGHT) continue;
            platform_display_put_pixel(x, y, mark == 'W' ? 0x00FFFFFFu : 0x00030A16u);
        }
    }
}

static void wm_render_desktop(void) {
    compositor_render_frame();
    wm_draw_desktop_icons();
    wm_draw_window_contents();
    if (fm_win && fm_win->active && !fm_win->minimized) file_manager_render();
    wm_draw_application_center();
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
        wm_open_application_center(0u, 0u);
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

    if (win == app_win) {
        if (app_view_detail) {
            if (y >= app_win->y + app_win->height - 70u) {
                app_view_detail = 0;
                return;
            }
        } else if (y >= app_win->y + 94u && y < app_win->y + 94u + APP_CENTER_PAGE_SIZE * 38u) {
            uint32_t index = app_page * APP_CENTER_PAGE_SIZE + (y - app_win->y - 94u) / 38u;
            const platform_app_info_t* app = 0;
            if (platform_app_get(index, &app) == PLATFORM_OK && app) {
                app_selected_index = index;
                if (app->id && app->id[0] == 't' && app->id[1] == 'e') {
                    app_win->active = 0; wm_restore_window(term_win);
                } else if (app->id && app->id[0] == 'f' && app->id[1] == 'i') {
                    app_win->active = 0; file_manager_open_window("/home/user"); fm_win = file_manager_get_window(); wm_restore_window(fm_win);
                } else if (app->id && app->id[0] == 's' && app->id[1] == 'y') {
                    app_win->active = 0; wm_restore_window(sec_win);
                } else wm_open_application_center(index, 1u);
            }
        }
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
    if (key == 'a' || key == 'A') { wm_open_application_center(0u, 0u); return; }
    if (key == 'w' || key == 'W') {
        for (uint32_t i = 0; i < platform_app_count(); ++i) {
            const platform_app_info_t* app = 0;
            if (platform_app_get(i, &app) == PLATFORM_OK && app && app->id &&
                app->id[0] == 'a' && app->id[1] == 'm' && app->id[2] == 'i' &&
                app->id[3] == 'n' && app->id[4] == 'e' && app->id[5] == 'v' &&
                app->id[6] == 'a' && app->id[7] == '-' && app->id[8] == 'w') {
                wm_open_application_center(i, 1u); return;
            }
        }
        return;
    }
    if ((key == 'b' || key == 'B') && app_win && app_win->active && app_view_detail) { app_view_detail = 0; return; }
    if (app_win && app_win->active && !app_view_detail && (key == '[' || key == ']')) {
        uint32_t pages = (platform_app_count() + APP_CENTER_PAGE_SIZE - 1u) / APP_CENTER_PAGE_SIZE;
        if (key == '[' && app_page > 0) --app_page;
        if (key == ']' && app_page + 1u < pages) ++app_page;
        return;
    }
    if ((key == 'q' || key == 'Q') && app_win && app_win->active) {
        app_win->active = 0;
        app_view_detail = 0;
        klog(LOG_INFO, "Application Center closed");
        wm_restore_window(term_win);
        return;
    }
    if (key == 't' || key == 'T') { wm_restore_window(term_win); return; }
    if (key == 's' || key == 'S') { wm_restore_window(sec_win); return; }

    /* Quick theme selectors make all built-in palettes testable from the desktop. */
    if (key == '1') { theme_set(THEME_MIDNIGHT_GOLD); return; }
    if (key == '2') { theme_set(THEME_CYBER_SAPPHIRE); return; }
    if (key == '3') { theme_set(THEME_EMERALD_DARK); return; }

    if (focused_win == app_win && app_win && app_win->active) return;
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
