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
static window_t* settings_win = 0;
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
    sec_win = compositor_create_window("SYSTEM STATUS", 170, 374, 832, 310, 0x00020617);
    app_win = compositor_create_window("APPLICATION CENTER", 150, 64, 800, 610, 0x00020617);
    if (app_win) app_win->active = 0;
    settings_win = compositor_create_window("SETTINGS", 138, 54, 846, 630, 0x00020617);
    if (settings_win) settings_win->active = 0;
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
        font_draw_string("Storage: temporary RAM-backed VFS", sec_win->x + 16, sec_win->y + 78, 0x0034D399, 0);
        font_draw_string("Input: detected keyboard and PS/2 pointer", sec_win->x + 16, sec_win->y + 100, 0x0034D399, 0);
        font_draw_string("Press A for Phoxtra and Amineva application entries.", sec_win->x + 16, sec_win->y + 122, 0x0094A3B8, 0);
    }
}

static void wm_redraw_window_frame(window_t* win, const theme_palette_t* palette) {
    if (!win || !palette) return;
    platform_display_fill_rect(win->x, win->y, win->width, win->height, win->bg_color);
    platform_display_draw_rect(win->x, win->y, win->width, win->height, 0x00334155u);
    platform_display_fill_rect(win->x, win->y, win->width, 32u, palette->titlebar_bg);
    platform_display_draw_rect(win->x, win->y, win->width, 32u, 0x00475569u);
    platform_display_fill_rect(win->x + 12u, win->y + 10u, 12u, 12u, 0x00B94A50u);
    platform_display_fill_rect(win->x + 30u, win->y + 10u, 12u, 12u, 0x00C8A34Au);
    platform_display_fill_rect(win->x + 48u, win->y + 10u, 12u, 12u, 0x00385F8Cu);
    font_draw_string_bold(win->title, win->x + 70u, win->y + 8u, 0x00F8FAFCu, 0);
}

static void wm_draw_settings(void) {
    if (!settings_win || !settings_win->active || settings_win->minimized) return;

    theme_palette_t* palette = theme_get_current();
    wm_redraw_window_frame(settings_win, palette);
    static int settings_render_logged = 0;
    if (!settings_render_logged) {
        klog(LOG_INFO, "Phoxtra Settings panel content rendered");
        settings_render_logged = 1;
    }
    uint32_t current = theme_get_current_id();
    uint32_t left_x = settings_win->x + 24u;
    uint32_t content_y = settings_win->y + 48u;
    uint32_t info_x = settings_win->x + 410u;
    static const char* const names[3] = {
        "MIDNIGHT GOLD", "CYBER SAPPHIRE", "EMERALD DARK"
    };

    font_draw_string_bold("PERSONALIZATION", left_x, content_y, palette->accent_primary, 0);
    font_draw_string("Choose a desktop theme:", left_x, content_y + 24u, palette->text_color, 0);
    for (uint32_t i = 0; i < 3u; ++i) {
        uint32_t row_y = settings_win->y + 112u + i * 54u;
        uint32_t row_color = (i == current) ? palette->titlebar_bg : palette->window_bg;
        platform_display_fill_rect(left_x, row_y, 360u, 42u, row_color);
        platform_display_draw_rect(left_x, row_y, 360u, 42u,
                                   (i == current) ? palette->accent_primary : 0x00334155u);
        font_draw_string_bold(names[i], left_x + 12u, row_y + 13u, palette->text_color, 0);
        font_draw_string((i == current) ? "CURRENT" : "CLICK TO APPLY",
                         left_x + 214u, row_y + 13u,
                         (i == current) ? palette->accent_primary : palette->accent_secondary, 0);
    }

    font_draw_string_bold("DEVICE STATUS", info_x, content_y, palette->accent_primary, 0);
    font_draw_string(platform_display_is_available() ? "Framebuffer: available" : "Framebuffer: unavailable",
                     info_x, content_y + 28u, palette->text_color, 0);
    font_draw_string("Display profile: 1024 x 768", info_x, content_y + 54u, palette->text_color, 0);
    font_draw_string(platform_input_has_keyboard() ? "Keyboard: detected" : "Keyboard: unavailable",
                     info_x, content_y + 80u, palette->text_color, 0);
    font_draw_string(platform_input_has_pointer() ? "Pointer path: detected" : "Pointer path: unavailable",
                     info_x, content_y + 106u, palette->text_color, 0);

    font_draw_string_bold("FONT STATUS", info_x, content_y + 132u, palette->accent_primary, 0);
    font_draw_string(font_is_ready() ? "Carlito / Calibri-compatible: ready"
                                     : "Carlito font: unavailable",
                     info_x, content_y + 154u, palette->text_color, 0);

    font_draw_string_bold("NETWORK STATUS", info_x, content_y + 184u, palette->accent_primary, 0);
    netdev_t* network = netdev_get_default();
    if (!network) {
        font_draw_string("Adapter: unavailable", info_x, content_y + 208u, palette->text_color, 0);
        font_draw_string("No supported driver registered.", info_x, content_y + 232u, palette->text_color, 0);
    } else {
        font_draw_string(network->name, info_x, content_y + 208u, palette->text_color, 0);
        font_draw_string((network->flags & 0x01u) ? "Link: UP" : "Link: DOWN",
                         info_x + 72u, content_y + 208u, palette->text_color, 0);
        if (network->ip_addr) {
            char ipv4[16];
            uint32_to_ip(network->ip_addr, ipv4);
            font_draw_string("IPv4:", info_x, content_y + 232u, palette->text_color, 0);
            font_draw_string(ipv4, info_x + 48u, content_y + 232u, palette->text_color, 0);
        } else {
            font_draw_string("IPv4: not configured", info_x, content_y + 232u, palette->text_color, 0);
        }
        if (network->dns_server) {
            char dns_ip[16];
            uint32_to_ip(network->dns_server, dns_ip);
            font_draw_string("DNS:", info_x, content_y + 256u, palette->text_color, 0);
            font_draw_string(dns_ip, info_x + 40u, content_y + 256u, palette->text_color, 0);
        } else {
            font_draw_string("DNS: not configured", info_x, content_y + 256u, palette->text_color, 0);
        }
    }

    font_draw_string_bold("STORAGE AND SAFETY", info_x, content_y + 282u, palette->accent_primary, 0);
    font_draw_string("Demo files: volatile RAM; lost at reboot.",
                     info_x, content_y + 310u, palette->text_color, 0);
    font_draw_string("Physical disk access is read-only.",
                     info_x, content_y + 336u, palette->text_color, 0);
    font_draw_string("Installer and formatting are disabled.",
                     info_x, content_y + 362u, palette->text_color, 0);

    font_draw_string_bold("SETTINGS AVAILABLE IN THIS BUILD",
                          left_x, content_y + 278u, palette->accent_primary, 0);
    font_draw_string("Desktop theme selection is configurable.",
                     left_x, content_y + 306u, palette->text_color, 0);
    font_draw_string("Network status is visible; configuration UI,",
                     left_x, content_y + 332u, palette->text_color, 0);
    font_draw_string("Wi-Fi/other NICs, accounts, sound, power and",
                     left_x, content_y + 358u, palette->text_color, 0);
    font_draw_string("software-update controls remain unavailable.",
                     left_x, content_y + 384u, palette->text_color, 0);
    font_draw_string("Press 1 / 2 / 3 to switch themes, or click a row.",
                     left_x, content_y + 422u, palette->accent_secondary, 0);
    font_draw_string("P opens Settings; Q closes this panel.",
                     left_x, content_y + 448u, palette->accent_secondary, 0);
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
    wm_redraw_window_frame(app_win, palette);
    static int app_render_logged = 0;
    if (!app_render_logged) {
        klog(LOG_INFO, "Phoxtra Application Center content rendered");
        app_render_logged = 1;
    }
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
    uint32_t page_count = (total + APP_CENTER_PAGE_SIZE - 1u) / APP_CENTER_PAGE_SIZE;
    if (page_count == 0u) page_count = 1u;
    char page_label[32];
    page_label[0] = 'P'; page_label[1] = 'a'; page_label[2] = 'g'; page_label[3] = 'e'; page_label[4] = ' ';
    uint32_t page_number = app_page + 1u;
    uint32_t pos = 5u;
    if (page_number >= 10u) page_label[pos++] = (char)('0' + (page_number / 10u));
    page_label[pos++] = (char)('0' + (page_number % 10u));
    page_label[pos++] = '/';
    if (page_count >= 10u) page_label[pos++] = (char)('0' + (page_count / 10u));
    page_label[pos++] = (char)('0' + (page_count % 10u));
    page_label[pos] = '\\0';
    font_draw_string(page_label, app_win->x + 24u, app_win->y + 500u, palette->accent_secondary, 0);
    font_draw_string("[: previous page  ]: next page  W: Workspace  Q: close", app_win->x + 24u, app_win->y + 526u, palette->accent_secondary, 0);
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
    wm_draw_application_center();
    wm_draw_settings();
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

static void wm_open_settings(void) {
    if (!settings_win) return;
    if (app_win) {
        app_win->active = 0;
        app_view_detail = 0;
    }
    settings_win->active = 1;
    settings_win->minimized = 0;
    compositor_raise_window(settings_win);
    focused_win = settings_win;
    klog(LOG_INFO, "Phoxtra Settings panel opened");
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
    } else if (x < 520) {
        wm_open_settings();
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
                } else if (app->id && app->id[0] == 's' && app->id[1] == 'e') {
                    app_win->active = 0; wm_open_settings();
                } else if (app->id && app->id[0] == 's' && app->id[1] == 'y') {
                    app_win->active = 0; wm_restore_window(sec_win);
                } else wm_open_application_center(index, 1u);
            }
        }
        return;
    }

    if (win == settings_win) {
        uint32_t row_x = settings_win->x + 24u;
        for (uint32_t i = 0; i < 3u; ++i) {
            uint32_t row_y = settings_win->y + 112u + i * 54u;
            if (x >= row_x && x < row_x + 360u &&
                y >= row_y && y < row_y + 42u) {
                theme_set(i);
                return;
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
    if (key == 'a' || key == 'A') { wm_open_application_center(0u, 0u); return; }
    if (key == 'p' || key == 'P') { wm_open_settings(); return; }
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
    if ((key == 'q' || key == 'Q') && settings_win && settings_win->active) {
        settings_win->active = 0;
        klog(LOG_INFO, "Phoxtra Settings panel closed");
        wm_restore_window(term_win);
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
        net_poll();
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
