#include "serial.hpp"
#include "console.hpp"
#include "init.hpp"
extern "C" {
#include "../drivers/framebuffer.h"
#include "../memory/pmm.h"
}

extern "C" {
#include "shell.h"
#include "boot_screen.h"
#include "../ui/wm.h"
}

extern "C" void kernel_init(uint32_t multiboot_magic, uint32_t multiboot_info_phys)
{
    serial::init();
    serial::write("[BOOT] kernel_init reached\n");
    framebuffer_set_boot_info(multiboot_magic, multiboot_info_phys);
    pmm_set_boot_info(multiboot_magic, multiboot_info_phys);

    system_init();

    serial::write("[BOOT] System initialization complete\n");

    asm volatile("sti");

    if (framebuffer_is_available()) {
        serial::write("[BOOT] Launching Phoxtra desktop by default\n");
        wm_start_desktop();
        wm_desktop_event_loop();
        serial::write("[BOOT] Desktop closed; opening recovery shell\n");
    }

    shell_start();

    while (true)
    {
        asm volatile("hlt");
    }
}

