#ifndef PHOXTRA_HAL_H
#define PHOXTRA_HAL_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    void (*disable_interrupts)(void);
    void (*enable_interrupts)(void);
    void (*halt)(void);
} hal_cpu_ops_t;

typedef struct {
    void (*init)(uint32_t frequency);
    uint64_t (*get_ticks)(void);
} hal_timer_ops_t;

typedef struct {
    void (*init)(void);
    char (*get_char)(void);
} hal_keyboard_ops_t;

typedef struct {
    void (*clear)(void);
    void (*write)(const char* text);
    void (*put_char)(char c);
} hal_display_ops_t;

typedef struct {
    hal_cpu_ops_t cpu;
    hal_timer_ops_t timer;
    hal_keyboard_ops_t keyboard;
    hal_display_ops_t display;
} hal_device_table_t;

void hal_init(void);
hal_device_table_t* hal_get_devices(void);

// Helper convenience macros/wrappers
void hal_cpu_disable_interrupts(void);
void hal_cpu_enable_interrupts(void);
void hal_cpu_halt(void);

uint64_t hal_timer_get_ticks(void);
char hal_keyboard_get_char(void);

void hal_display_clear(void);
void hal_display_write(const char* text);
void hal_display_put_char(char c);

#ifdef __cplusplus
}
#endif

#endif
