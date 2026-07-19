#include "serial.hpp"
#include "console.hpp"
#include "init.hpp"

extern "C" void kernel_init()
{
    serial::init();
    serial::write("[BOOT] kernel_init reached\n");

    console::clear();

    console::write("Akwaaba\n\n");

    console::write("PHOXTRA OS\n");
    console::write("Secure Identity Operating System\n\n");

    console::write("CPU ............... OK\n");
    console::write("Memory ............ OK\n");
    console::write("Security Core ..... Loading\n");

    serial::write("[BOOT] Console initialized\n");

    system_init();

    serial::write("[BOOT] System initialization complete\n");

    while (true)
    {
        asm volatile("hlt");
    }
}
