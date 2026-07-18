#include "kernel.hpp"
#include "init.hpp"
#include "console.hpp"
#include "serial.hpp"

extern "C" void kernel_init()
{
    console::clear();

    console::write("Akwaaba\n\n");

    console::write("PHOXTRA OS\n");
    console::write("Secure Identity Operating System\n\n");

    console::write("Initializing CPU ........ OK\n");
    console::write("Loading Kernel .......... OK\n");
    console::write("Starting Security Core .. WAITING\n");

    serial::init();
    serial::write("Akwaaba - Serial Console Started\n");

    system_init();

    while (true)
    {
        asm volatile("hlt");
    }
}
