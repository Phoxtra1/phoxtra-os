#include "kernel.hpp"
#include "init.hpp"

extern "C" void kernel_init()
{
    system_init();

    while (true)
    {
        asm volatile("hlt");
    }
}
