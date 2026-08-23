#ifndef PHOXTRA_BOOT_SCREEN_H
#define PHOXTRA_BOOT_SCREEN_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * boot_screen_banner()
 *   Renders the Phoxtra OS identity banner to the VGA console.
 *   Must be called after the console is initialized and before
 *   any boot status messages are printed.
 *
 *   Output:
 *   =================================================
 *                  PHOXTRA OS
 *         Secure Identity Operating System
 *                  v0.5.1-alpha
 *   =================================================
 */
void boot_screen_banner(void);
void boot_screen_display(void);
void boot_screen_section_boot(void);
void boot_screen_section_kernel(void);
void boot_screen_ready(void);


#ifdef __cplusplus
}
#endif

#endif
