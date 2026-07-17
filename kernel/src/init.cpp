#include "init.hpp"
#include "../arch/x86_64/include/cpu.hpp"

void system_init()
{
    cpu_init();

    // Future kernel subsystems:
    // memory_init();
    // interrupt_init();
    // scheduler_init();
    // security_init();
}
