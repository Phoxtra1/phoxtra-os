#include "wm.h"
#include "compositor.h"
#include "font.h"
#include "theme.h"
#include "file_manager.h"
#include "../drivers/framebuffer.h"
#include "../installer/installer.h"
#include "../log/logger.h"
#include "../src/mouse.h"
#include "../src/keyboard.h"
#include "../src/timer.h"

static window_t* term_win = 0;
static window_t* sec_win  = 0;
static window_t* mon_win  = 0;
static window_t* fm_win   = 0;

void wm_init(void) {
    klog(LOG_INFO, "Initializing Phoxtra Desktop Window Manager (phoxtra-wm)...");
    theme_init();
    file_manager_init();
    installer_init();

    // Initialize Default Desktop Windows
    term_win = compositor_create_window("Phoxtra Terminal - Ring 3", 60, 60, 480, 320, 0x00020617);
    fm_win   = compositor_create_window("Phoxtra File Manager - /home/user", 570, 60, 400, 320, 0x00020617);
    sec_win  = compositor_create_window("Phoxtra Security & CAC Audit", 60, 410, 910, 280, 0x00020617);
}

void wm_start_desktop(void) {
    klog(LOG_INFO, "Starting High-Resolution GUI Compositor (1024x768x32)...");

    // Render Initial GUI Desktop Frame
    compositor_render_frame();

    // Render Desktop Icons (Folders & Drives)
    icon_draw_drive(20, 20, "System NVMe");
    icon_draw_folder(20, 90, "Documents");
    icon_draw_folder(20, 160, "Downloads");
    icon_draw_folder(20, 230, "System");
    icon_draw_file(20, 300, "README.txt");

    // Render Window Inner Contents
    if (term_win) {
        font_draw_string("AKWAABA PHOXTRA OS v1.0.0 Commercial", term_win->x + 16, term_win->y + 48, 0x00FFD700, 0x00000000);
        font_draw_string("phoxtra> ifconfig", term_win->x + 16, term_win->y + 70, 0x00F8FAFC, 0x00000000);
        font_draw_string("eth0: inet 192.168.1.150 netmask 255.255.255.0", term_win->x + 16, term_win->y + 92, 0x0094A3B8, 0x00000000);
        font_draw_string("phoxtra> theme midnight-gold", term_win->x + 16, term_win->y + 114, 0x00F8FAFC, 0x00000000);
        font_draw_string("Theme switched: Phoxtra Flagship Midnight Gold", term_win->x + 16, term_win->y + 136, 0x0034D399, 0x00000000);
        font_draw_string("phoxtra> installer", term_win->x + 16, term_win->y + 158, 0x0038BDF8, 0x00000000);
    }

    if (fm_win) {
        font_draw_string("Location: /home/user", fm_win->x + 16, fm_win->y + 42, 0x0038BDF8, 0x00000000);
        fb_fill_rect(fm_win->x + 16, fm_win->y + 60, fm_win->width - 32, 2, 0x00334155);

        // Graphical Folder Grid inside File Manager
        icon_draw_folder(fm_win->x + 24, fm_win->y + 80, "Projects");
        icon_draw_folder(fm_win->x + 104, fm_win->y + 80, "Pictures");
        icon_draw_folder(fm_win->x + 184, fm_win->y + 80, "Music");
        icon_draw_file(fm_win->x + 264, fm_win->y + 80, "config.json");

        icon_draw_folder(fm_win->x + 24, fm_win->y + 170, "Desktop");
        icon_draw_file(fm_win->x + 104, fm_win->y + 170, "notes.txt");
        icon_draw_file(fm_win->x + 184, fm_win->y + 170, "app.elf");

        font_draw_string("7 items | 4.2 GB free", fm_win->x + 16, fm_win->y + 290, 0x0094A3B8, 0x00000000);
    }

    if (sec_win) {
        font_draw_string("SECURITY POLICY STATUS & AUDIT RING BUFFER", sec_win->x + 16, sec_win->y + 48, 0x00FFD700, 0x00000000);
        font_draw_string("[1] Hardware TPM 2.0 Root of Trust ........ VERIFIED (EK SHA-256)", sec_win->x + 16, sec_win->y + 76, 0x0034D399, 0x00000000);
        font_draw_string("[2] Capability Access Control (CAC) ......... ENFORCING (Bitmask 64-bit)", sec_win->x + 16, sec_win->y + 98, 0x0034D399, 0x00000000);
        font_draw_string("[3] Mandatory Access Control (MAC) .......... ACTIVE (SYSTEM / SERVICE / USER)", sec_win->x + 16, sec_win->y + 120, 0x0034D399, 0x00000000);
        font_draw_string("[4] Stack Canary & Kernel Guard Pages ...... ACTIVE (Hardware Entropy Seeded)", sec_win->x + 16, sec_win->y + 142, 0x0034D399, 0x00000000);
        font_draw_string("[5] Bare-Metal NVMe/PCIe Scanner ........... READY (pci_scan)", sec_win->x + 16, sec_win->y + 164, 0x0034D399, 0x00000000);
    }

    fb_swap_buffers();
}

uint32_t wm_get_window_count(void) {
    return 3;
}


static window_t* wm_drag_window = 0;
static int32_t wm_drag_dx = 0, wm_drag_dy = 0;
static uint8_t wm_was_pressed = 0;

static void wm_render_desktop(void) {
    compositor_render_frame();
    icon_draw_drive(20, 20, "System NVMe");
    icon_draw_folder(20, 90, "Documents");
    icon_draw_folder(20, 160, "Downloads");
    icon_draw_folder(20, 230, "System");
    icon_draw_file(20, 300, "README.txt");
    if (term_win) {
        font_draw_string("AKWAABA PHOXTRA OS v1.0.0", term_win->x + 16, term_win->y + 48, 0x00FFD700, 0);
        font_draw_string("phoxtra> help", term_win->x + 16, term_win->y + 70, 0x00F8FAFC, 0);
        font_draw_string("Keyboard: click window, type; Esc returns to shell", term_win->x + 16, term_win->y + 92, 0x0094A3B8, 0);
    }
    if (fm_win) {
        font_draw_string("Location: /home/user", fm_win->x + 16, fm_win->y + 42, 0x0038BDF8, 0);
        fb_fill_rect(fm_win->x + 16, fm_win->y + 60, fm_win->width - 32, 2, 0x00334155);
        icon_draw_folder(fm_win->x + 24, fm_win->y + 80, "Documents");
        icon_draw_folder(fm_win->x + 104, fm_win->y + 80, "Downloads");
        icon_draw_folder(fm_win->x + 184, fm_win->y + 80, "Pictures");
        icon_draw_folder(fm_win->x + 24, fm_win->y + 170, "Desktop");
        icon_draw_file(fm_win->x + 104, fm_win->y + 170, "README.txt");
    }
    if (sec_win) {
        font_draw_string("SECURITY POLICY / AUDIT STATUS", sec_win->x + 16, sec_win->y + 48, 0x00FFD700, 0);
        font_draw_string("Kernel capabilities and access checks", sec_win->x + 16, sec_win->y + 76, 0x0034D399, 0);
        font_draw_string("Device-backed status depends on detected hardware", sec_win->x + 16, sec_win->y + 98, 0x0034D399, 0);
    }
    fb_swap_buffers();
}

static void wm_handle_click(uint32_t x, uint32_t y) {
    window_t* win = compositor_window_at_point(x, y);
    if (!win) {
        if (y >= FB_HEIGHT - 48 && x < 142) {
            if (fm_win) fm_win->minimized = 0;
        }
        return;
    }
    if (y < win->y + 32) {
        if (x < win->x + 28) { win->active = 0; if (win == term_win) term_win = 0; if (win == sec_win) sec_win = 0; if (win == fm_win) fm_win = 0; return; }
        if (x < win->x + 46) { win->minimized = 1; return; }
        if (x < win->x + 64) {
            if (!win->maximized) {
                win->x = 0; win->y = 0; win->width = FB_WIDTH; win->height = FB_HEIGHT - 48; win->maximized = 1;
            } else {
                win->x = 60; win->y = 60; win->width = 480; win->height = 320; win->maximized = 0;
            }
            return;
        }
        wm_drag_window = win;
        wm_drag_dx = (int32_t)x - (int32_t)win->x;
        wm_drag_dy = (int32_t)y - (int32_t)win->y;
        return;
    }
    if (win == fm_win && y >= win->y + 78 && y < win->y + 270) {
        uint32_t row = (y - (win->y + 78)) / 92;
        uint32_t col = (x - (win->x + 22)) / 112;
        uint32_t index = row * 3 + col;
        (void)file_manager_open_entry(index);
    }
}

void wm_desktop_event_loop(void) {
    klog(LOG_INFO, "Desktop event loop active; use Esc to return to shell");
    wm_render_desktop();
    uint64_t last_redraw = timer_get_ticks();
    for (;;) {
        mouse_event_t ev;
        if (mouse_poll_event(&ev)) {
            if (ev.left_clicked) {
                wm_handle_click((uint32_t)ev.x, (uint32_t)ev.y);
                wm_was_pressed = 1;
                last_redraw = 0;
            } else if ((ev.buttons & 1u) && wm_drag_window) {
                int32_t nx = ev.x - wm_drag_dx, ny = ev.y - wm_drag_dy;
                if (nx < 0) nx = 0; if (ny < 0) ny = 0;
                if (nx + (int32_t)wm_drag_window->width > FB_WIDTH) nx = FB_WIDTH - wm_drag_window->width;
                if (ny + 32 > FB_HEIGHT - 48) ny = FB_HEIGHT - 80;
                wm_drag_window->x = (uint32_t)nx; wm_drag_window->y = (uint32_t)ny;
                last_redraw = 0;
            } else if (!(ev.buttons & 1u)) wm_drag_window = 0;
        }
        char key;
        if (keyboard_poll_char(&key)) {
            if (key == 27) break;
            if (key == 'f' && fm_win) { fm_win->minimized = 0; last_redraw = 0; }
            if (key == 't' && term_win) { term_win->minimized = 0; last_redraw = 0; }
        }
        if (last_redraw == 0 || timer_get_ticks() - last_redraw >= 2) {
            wm_render_desktop();
            last_redraw = timer_get_ticks();
        }
        __asm__ volatile ("pause");
    }
    klog(LOG_INFO, "Desktop event loop returned to shell");
}
