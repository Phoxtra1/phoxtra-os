#include "serial.hpp"
#include "console.hpp"
#include "init.hpp"

extern "C" {
#include "shell.h"
#include "boot_screen.h"
}

extern "C" void kernel_init()
{
    serial::init();
    serial::write("[BOOT] kernel_init reached\n");

    system_init();

    serial::write("[BOOT] System initialization complete\n");

    asm volatile("sti");

    shell_start();

    while (true)
    {
        asm volatile("hlt");
    }
}

