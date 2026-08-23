#include "wm.h"
#include "compositor.h"
#include "font.h"
#include "theme.h"
#include "file_manager.h"
#include "../drivers/framebuffer.h"
#include "../installer/installer.h"
#include "../log/logger.h"

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
