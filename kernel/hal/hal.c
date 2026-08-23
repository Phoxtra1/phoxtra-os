#include "hal.h"
#include "../include/timer.h"
#include "../include/keyboard.h"
#include "../log/logger.h"

// External console wrappers declared in console.hpp
extern void console_clear(void);
extern void console_write(const char* text);
extern void console_put_char(char c);

static void hal_cpu_disable_impl(void) { asm volatile("cli"); }
static void hal_cpu_enable_impl(void) { asm volatile("sti"); }
static void hal_cpu_halt_impl(void) { asm volatile("hlt"); }

static hal_device_table_t hal_devices;

void hal_init(void) {
    hal_devices.cpu.disable_interrupts = hal_cpu_disable_impl;
    hal_devices.cpu.enable_interrupts  = hal_cpu_enable_impl;
    hal_devices.cpu.halt               = hal_cpu_halt_impl;

    hal_devices.timer.init             = timer_init;
    hal_devices.timer.get_ticks        = timer_get_ticks;

    hal_devices.keyboard.init          = keyboard_init;
    hal_devices.keyboard.get_char      = keyboard_get_char;

    hal_devices.display.clear          = console_clear;
    hal_devices.display.write          = console_write;
    hal_devices.display.put_char       = console_put_char;

    klog(LOG_KERNEL, "Hardware Abstraction Layer (HAL) started");
}

hal_device_table_t* hal_get_devices(void) {
    return &hal_devices;
}

void hal_cpu_disable_interrupts(void) {
    if (hal_devices.cpu.disable_interrupts) hal_devices.cpu.disable_interrupts();
}

void hal_cpu_enable_interrupts(void) {
    if (hal_devices.cpu.enable_interrupts) hal_devices.cpu.enable_interrupts();
}

void hal_cpu_halt(void) {
    if (hal_devices.cpu.halt) hal_devices.cpu.halt();
}

uint64_t hal_timer_get_ticks(void) {
    if (hal_devices.timer.get_ticks) return hal_devices.timer.get_ticks();
    return 0;
}

char hal_keyboard_get_char(void) {
    if (hal_devices.keyboard.get_char) return hal_devices.keyboard.get_char();
    return 0;
}

void hal_display_clear(void) {
    if (hal_devices.display.clear) hal_devices.display.clear();
}

void hal_display_write(const char* text) {
    if (hal_devices.display.write) hal_devices.display.write(text);
}

void hal_display_put_char(char c) {
    if (hal_devices.display.put_char) hal_devices.display.put_char(c);
}
