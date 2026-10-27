#ifndef PHOXTRA_FRAMEBUFFER_H
#define PHOXTRA_FRAMEBUFFER_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define FB_WIDTH 1024
#define FB_HEIGHT 768
#define FB_BPP 32
#define FB_PITCH (FB_WIDTH * 4)
#define MULTIBOOT2_BOOT_MAGIC 0x36D76289u
typedef struct framebuffer {
    uint32_t* address;
    uint32_t* back_buffer;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t bpp;
    uint32_t size;
    uint8_t available;
} framebuffer_t;
void framebuffer_set_boot_info(uint32_t magic, uint32_t info_phys);
void framebuffer_init(void);
framebuffer_t* framebuffer_get(void);
int framebuffer_is_available(void);
uint32_t fb_get_back_pixel(uint32_t x, uint32_t y);
void fb_put_pixel(uint32_t x, uint32_t y, uint32_t color);
void fb_draw_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color);
void fb_fill_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color);
void fb_draw_gradient(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color1, uint32_t color2);
void fb_clear(uint32_t color);
void fb_swap_buffers(void);
#ifdef __cplusplus
}
#endif
#endif
