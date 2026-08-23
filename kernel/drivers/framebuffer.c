#include "framebuffer.h"
#include "../memory/heap.h"
#include "../log/logger.h"

static framebuffer_t fb;
static uint32_t fb_storage[FB_WIDTH * FB_HEIGHT];
static uint32_t back_storage[FB_WIDTH * FB_HEIGHT];

void framebuffer_init(void) {
    klog(LOG_INFO, "Initializing VESA VBE / UEFI GOP Linear Framebuffer Graphics Driver...");

    fb.address     = fb_storage;
    fb.back_buffer = back_storage;
    fb.width       = FB_WIDTH;
    fb.height      = FB_HEIGHT;
    fb.bpp         = FB_BPP;
    fb.pitch       = FB_PITCH;
    fb.size        = FB_WIDTH * FB_HEIGHT * 4;

    fb_clear(0x000F172A); // Modern dark blue backdrop (Phoxtra Midnight Navy)
    fb_swap_buffers();

    klog(LOG_INFO, "Linear Framebuffer Initialized: 1024x768 @ 32bpp ARGB8888 (Double-buffered)");
}

framebuffer_t* framebuffer_get(void) {
    return &fb;
}

void fb_put_pixel(uint32_t x, uint32_t y, uint32_t color) {
    if (x >= FB_WIDTH || y >= FB_HEIGHT) return;
    fb.back_buffer[y * FB_WIDTH + x] = color;
}

void fb_draw_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color) {
    for (uint32_t i = x; i < x + width && i < FB_WIDTH; i++) {
        fb_put_pixel(i, y, color);
        fb_put_pixel(i, y + height - 1, color);
    }
    for (uint32_t j = y; j < y + height && j < FB_HEIGHT; j++) {
        fb_put_pixel(x, j, color);
        fb_put_pixel(x + width - 1, j, color);
    }
}

void fb_fill_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color) {
    for (uint32_t j = y; j < y + height && j < FB_HEIGHT; j++) {
        for (uint32_t i = x; i < x + width && i < FB_WIDTH; i++) {
            fb.back_buffer[j * FB_WIDTH + i] = color;
        }
    }
}

void fb_draw_gradient(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color1, uint32_t color2) {
    uint8_t r1 = (color1 >> 16) & 0xFF, g1 = (color1 >> 8) & 0xFF, b1 = color1 & 0xFF;
    uint8_t r2 = (color2 >> 16) & 0xFF, g2 = (color2 >> 8) & 0xFF, b2 = color2 & 0xFF;

    for (uint32_t j = 0; j < height && (y + j) < FB_HEIGHT; j++) {
        uint8_t r = r1 + ((r2 - r1) * j) / height;
        uint8_t g = g1 + ((g2 - g1) * j) / height;
        uint8_t b = b1 + ((b2 - b1) * j) / height;
        uint32_t color = (0xFF << 24) | (r << 16) | (g << 8) | b;

        for (uint32_t i = x; i < x + width && i < FB_WIDTH; i++) {
            fb.back_buffer[(y + j) * FB_WIDTH + i] = color;
        }
    }
}

void fb_clear(uint32_t color) {
    for (uint32_t i = 0; i < FB_WIDTH * FB_HEIGHT; i++) {
        fb.back_buffer[i] = color;
    }
}

void fb_swap_buffers(void) {
    for (uint32_t i = 0; i < FB_WIDTH * FB_HEIGHT; i++) {
        fb.address[i] = fb.back_buffer[i];
    }
}
