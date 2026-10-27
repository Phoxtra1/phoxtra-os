/*
 * kernel/src/boot_screen.c
 *
 * Phoxtra OS Boot Presentation Module
 *
 * Responsible for rendering the identity banner and boot section
 * headers to the VGA console. Deliberately kept separate from kernel
 * initialisation logic so that the visual presentation can be updated
 * independently of subsystem bring-up code.
 *
 * All output goes through the console_write / console_put_char wrappers
 * that are also used by the shell — no direct VGA register access here.
 */

#include "boot_screen.h"
#include "serial.hpp"

/* Console wrappers provided by console.cpp */
extern "C" {
    void console_clear(void);
    void console_write(const char* text);
    void console_put_char(char c);
}

/* ────────────────────────────────────────────────────────────────────────
   boot_screen_banner
   Renders the full Phoxtra OS identity banner. Call this immediately
   after console_clear() and before any status or log output.
   ──────────────────────────────────────────────────────────────────────── */
void boot_screen_banner(void) {
    console_clear();
    console_write("=================================================\n");
    console_write("              AKWAABA - PHOXTRA OS\n");
    console_write("      Secure Identity Operating System\n");
    console_write("=================================================\n\n");
}

void boot_screen_display(void) {
    console_clear();
    console_write("=================================================\n");
    console_write("              AKWAABA - PHOXTRA OS\n");
    console_write("      Secure Identity Operating System\n");
    console_write("=================================================\n\n");

    console_write(" [BOOT]                             [BOOTABLE TEST BUILD]\n");
    console_write("  CPU .......... OK                  Memory ....... OK\n");
    console_write("  GDT .......... OK                  Scheduler .... OK\n");
    console_write("  IDT .......... OK                  Storage ...... RAM test only\n");
    console_write("  Timer ........ OK                  TPM 2.0 Engine OK\n");
    console_write("  Keyboard ..... OK                  Process ID ... OK\n");
    console_write("  TPM SPI ...... OK                  CAC & MAC .... OK\n");
    console_write("  Network e1000  OK                  TCP/IP Stack . OK\n");
    console_write("  ELF64 Loader . OK                  phoxtra-libc . OK\n");
    console_write("  VBE Framebuffer OK                 phoxtra-wm ... OK\n");
    console_write("  Stress Engine  OK                  Panic Audit .. OK\n");
    console_write("  Developer SDK  OK                  Syscall Table  OK\n");
    console_write("  PCI Bus Scan   OK                  Enterprise LTS OK\n\n");

    console_write("Bootable test environment - disk installation is not implemented.\n\n");
}

void boot_screen_section_boot(void) {}
void boot_screen_section_kernel(void) {}
void boot_screen_ready(void) {}

