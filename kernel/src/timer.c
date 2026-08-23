#include "timer.h"
#include "irq.h"
#include "../log/logger.h"

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

volatile uint64_t timer_ticks = 0;

/* Forward declaration — scheduler_tick() is defined in kernel/scheduler/scheduler.c */
extern void scheduler_tick(void);

static void timer_callback(struct registers* regs) {
    (void)regs;
    timer_ticks++;
    scheduler_tick();
}

void timer_init(uint32_t frequency) {
    timer_ticks = 0;

    uint32_t divisor = 1193182 / frequency;

    outb(0x43, 0x36);

    uint8_t l = (uint8_t)(divisor & 0xFF);
    uint8_t h = (uint8_t)((divisor >> 8) & 0xFF);

    outb(0x40, l);
    outb(0x40, h);

    irq_install_handler(0, timer_callback);
    klog(LOG_INFO, "Timer initialized at 100 Hz");
}

uint64_t timer_get_ticks(void) {
    return timer_ticks;
}
