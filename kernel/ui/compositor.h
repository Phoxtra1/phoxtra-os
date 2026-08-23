#ifndef PHOXTRA_COMPOSITOR_H
#define PHOXTRA_COMPOSITOR_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct window {
    uint32_t id;
    char title[64];
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
    uint32_t bg_color;
    uint8_t  active;
    uint8_t  minimized;
} window_t;

void compositor_init(void);
window_t* compositor_create_window(const char* title, uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t bg_color);
void compositor_render_frame(void);

#ifdef __cplusplus
}
#endif

#endif
