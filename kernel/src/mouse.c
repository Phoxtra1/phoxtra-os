#include "mouse.h"
#include "irq.h"
#include "logger.h"
#include "../drivers/framebuffer.h"

#define PS2_STATUS_PORT 0x64
#define PS2_COMMAND_PORT 0x64
#define PS2_DATA_PORT 0x60
#define PS2_AUX_WRITE 0xD4
#define PS2_AUX_ENABLE 0xA8
#define PS2_READ_COMMAND_BYTE 0x20
#define PS2_WRITE_COMMAND_BYTE 0x60
#define PS2_MOUSE_DEFAULTS 0xF6
#define PS2_MOUSE_ENABLE_STREAMING 0xF4
#define PS2_ACK 0xFA
#define PS2_WAIT_LIMIT 100000u

static volatile int32_t pointer_x = FB_WIDTH / 2;
static volatile int32_t pointer_y = FB_HEIGHT / 2;
static volatile uint8_t pointer_buttons = 0;
static volatile uint8_t packet[3];
static volatile uint8_t packet_index = 0;
static volatile uint8_t click_pending = 0;
static volatile uint8_t event_pending = 0;
static int mouse_ready = 0;

static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static int wait_input_empty(void) {
    for (uint32_t i = 0; i < PS2_WAIT_LIMIT; ++i)
        if ((inb(PS2_STATUS_PORT) & 0x02u) == 0) return 1;
    return 0;
}

static int wait_output_full(uint8_t* value) {
    for (uint32_t i = 0; i < PS2_WAIT_LIMIT; ++i) {
        if (inb(PS2_STATUS_PORT) & 0x01u) {
            if (value) *value = inb(PS2_DATA_PORT);
            else (void)inb(PS2_DATA_PORT);
            return 1;
        }
    }
    return 0;
}

static int write_command_byte(uint8_t value) {
    if (!wait_input_empty()) return 0;
    outb(PS2_COMMAND_PORT, PS2_WRITE_COMMAND_BYTE);
    if (!wait_input_empty()) return 0;
    outb(PS2_DATA_PORT, value);
    return 1;
}

static int mouse_command(uint8_t command) {
    uint8_t response = 0;
    if (!wait_input_empty()) return 0;
    outb(PS2_COMMAND_PORT, PS2_AUX_WRITE);
    if (!wait_input_empty()) return 0;
    outb(PS2_DATA_PORT, command);
    if (!wait_output_full(&response)) return 0;
    return response == PS2_ACK;
}

static void mouse_irq(struct registers* regs) {
    (void)regs;
    const uint8_t value = inb(PS2_DATA_PORT);

    if (packet_index == 0 && (value & 0x08u) == 0) return;
    packet[packet_index++] = value;
    if (packet_index < 3) return;
    packet_index = 0;

    const uint8_t flags = packet[0];
    if (flags & 0xC0u) return; /* Ignore overflowed movement packets. */

    int32_t next_x = pointer_x + (int8_t)packet[1];
    int32_t next_y = pointer_y - (int8_t)packet[2];
    if (next_x < 0) next_x = 0;
    if (next_y < 0) next_y = 0;
    if (next_x >= FB_WIDTH) next_x = FB_WIDTH - 1;
    if (next_y >= FB_HEIGHT) next_y = FB_HEIGHT - 1;

    const uint8_t next_buttons = flags & 0x07u;
    if ((next_buttons & 0x01u) && !(pointer_buttons & 0x01u))
        click_pending = 1;

    pointer_x = next_x;
    pointer_y = next_y;
    pointer_buttons = next_buttons;
    event_pending = 1;
}

void mouse_init(void) {
    pointer_x = FB_WIDTH / 2;
    pointer_y = FB_HEIGHT / 2;
    pointer_buttons = 0;
    packet_index = 0;
    click_pending = 0;
    event_pending = 0;
    mouse_ready = 0;

    /* Enable the auxiliary PS/2 clock, but keep IRQ12 disabled while
       issuing setup commands so the command ACK remains available here. */
    if (!wait_input_empty()) {
        klog(LOG_WARNING, "PS/2 mouse unavailable: controller did not become ready");
        return;
    }
    outb(PS2_COMMAND_PORT, PS2_AUX_ENABLE);

    if (!wait_input_empty()) return;
    outb(PS2_COMMAND_PORT, PS2_READ_COMMAND_BYTE);
    uint8_t command_byte = 0;
    if (!wait_output_full(&command_byte)) {
        klog(LOG_WARNING, "PS/2 mouse unavailable: no controller response");
        return;
    }
    command_byte = (uint8_t)((command_byte | 0x20u) & (uint8_t)~0x02u);
    command_byte = (uint8_t)(command_byte & (uint8_t)~0x20u);
    if (!write_command_byte(command_byte)) {
        klog(LOG_WARNING, "PS/2 mouse unavailable: failed to configure controller");
        return;
    }

    if (!mouse_command(PS2_MOUSE_DEFAULTS)) {
        klog(LOG_WARNING, "PS/2 mouse unavailable: device did not acknowledge defaults");
        return;
    }
    if (!mouse_command(PS2_MOUSE_ENABLE_STREAMING)) {
        klog(LOG_WARNING, "PS/2 mouse unavailable: streaming could not be enabled");
        return;
    }

    command_byte = (uint8_t)((command_byte | 0x02u) & (uint8_t)~0x20u);
    if (!write_command_byte(command_byte)) {
        klog(LOG_WARNING, "PS/2 mouse unavailable: failed to enable IRQ12");
        return;
    }

    irq_install_handler(12, mouse_irq);
    mouse_ready = 1;
    klog(LOG_INFO, "PS/2 mouse initialized (IRQ12, 3-byte packets)");
}

int mouse_is_available(void) {
    return mouse_ready;
}

int mouse_poll_event(mouse_event_t* event) {
    if (!event || !mouse_ready) return 0;

    uint64_t flags;
    __asm__ volatile ("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
    if (!event_pending) {
        __asm__ volatile ("pushq %0; popfq" : : "r"(flags) : "memory", "cc");
        return 0;
    }
    event->x = pointer_x;
    event->y = pointer_y;
    event->buttons = pointer_buttons;
    event->left_clicked = click_pending;
    click_pending = 0;
    event_pending = 0;
    __asm__ volatile ("pushq %0; popfq" : : "r"(flags) : "memory", "cc");
    return 1;
}

void mouse_get_position(int32_t* x, int32_t* y) {
    uint64_t flags;
    __asm__ volatile ("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
    if (x) *x = pointer_x;
    if (y) *y = pointer_y;
    __asm__ volatile ("pushq %0; popfq" : : "r"(flags) : "memory", "cc");
}
