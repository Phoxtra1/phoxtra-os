#ifndef PHOXTRA_FONT_H
#define PHOXTRA_FONT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FONT_WIDTH  16
#define FONT_AVG_WIDTH 8
#define FONT_HEIGHT 22

void font_init(void);
void font_draw_char(char c, uint32_t x, uint32_t y, uint32_t fg_color, uint32_t bg_color);
void font_draw_char_bold(char c, uint32_t x, uint32_t y, uint32_t fg_color, uint32_t bg_color);
void font_draw_string(const char* str, uint32_t x, uint32_t y, uint32_t fg_color, uint32_t bg_color);
void font_draw_string_bold(const char* str, uint32_t x, uint32_t y, uint32_t fg_color, uint32_t bg_color);

#ifdef __cplusplus
}
#endif

#endif
