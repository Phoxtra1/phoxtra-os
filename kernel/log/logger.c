#include "logger.h"

static inline void outb(unsigned short port, unsigned char value)
{
    asm volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline unsigned char inb(unsigned short port)
{
    unsigned char value;
    asm volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static void serial_write_str(const char* str) {
    while (*str) {
        while (!(inb(0x3F8 + 5) & 0x20)) {}
        outb(0x3F8, *str++);
    }
}

void logger_init(void) {
    klog(LOG_INFO, "Logger system initialized");
}

void klog(log_level_t level, const char* message) {
    switch (level) {
        case LOG_INFO:
            serial_write_str("[INFO] ");
            break;
        case LOG_WARNING:
            serial_write_str("[WARNING] ");
            break;
        case LOG_ERROR:
            serial_write_str("[ERROR] ");
            break;
        case LOG_SECURITY:
            serial_write_str("[SECURITY] ");
            break;
        case LOG_KERNEL:
            serial_write_str("[KERNEL] ");
            break;
        case LOG_HARDWARE:
            serial_write_str("[HARDWARE] ");
            break;
        default:
            serial_write_str("[LOG] ");
            break;
    }
    serial_write_str(message);
    serial_write_str("\n");
}
