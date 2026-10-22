#include "font.h"
#include "font_carlito_data.h"
#include "../drivers/framebuffer.h"
#include "../log/logger.h"

#define GLYPH_COUNT 95u
#define GLYPH_BYTES 18u

static uint8_t glyph_data[GLYPH_COUNT][GLYPH_BYTES];
static int font_ready;

static int base64_value(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

static uint32_t decode_base64(const char* src, uint8_t* dst, uint32_t capacity) {
    uint32_t written = 0;
    uint32_t accumulator = 0;
    uint32_t bits = 0;
    while (*src) {
        int value = base64_value(*src++);
        if (value < 0) continue;
        accumulator = (accumulator << 6) | (uint32_t)value;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            if (written >= capacity) return 0;
            dst[written++] = (uint8_t)(accumulator >> bits);
        }
    }
    return written;
}

void font_init(void) {
    uint32_t decoded = decode_base64(phoxtra_carlito_regular_b64,
                                     &glyph_data[0][0], sizeof(glyph_data));
    font_ready = decoded == sizeof(glyph_data);
    klog(font_ready ? LOG_INFO : LOG_ERROR,
         font_ready ? "Carlito Calibri-compatible 9x16 font initialized"
                    : "Carlito font data invalid; text renderer disabled");
}

static void draw_char_style(char c, uint32_t x, uint32_t y,
                            uint32_t fg, uint32_t bg, int bold) {
    uint8_t code = (uint8_t)c;
    if (code < 32 || code > 126) code = '?';
    uint32_t index = (uint32_t)code - 32u;
    for (uint32_t row = 0; row < FONT_HEIGHT; ++row) {
        for (uint32_t col = 0; col < FONT_WIDTH; ++col) {
            uint32_t bit = row * FONT_WIDTH + col;
            uint8_t byte = glyph_data[index * GLYPH_BYTES + bit / 8u];
            int set = font_ready && ((byte >> (bit % 8u)) & 1u);
            if (bold && !set && col > 0) {
                uint32_t prev = bit - 1u;
                set = ((glyph_data[index * GLYPH_BYTES + prev / 8u] >> (prev % 8u)) & 1u) != 0;
            }
            if (set) {
                fb_put_pixel(x + col, y + row, fg);
                if (bold && col + 1u < FONT_WIDTH) fb_put_pixel(x + col + 1u, y + row, fg);
            } else if (bg != 0) {
                fb_put_pixel(x + col, y + row, bg);
            }
        }
    }
}

void font_draw_char(char c, uint32_t x, uint32_t y, uint32_t fg, uint32_t bg) {
    draw_char_style(c, x, y, fg, bg, 0);
}

void font_draw_char_bold(char c, uint32_t x, uint32_t y, uint32_t fg, uint32_t bg) {
    draw_char_style(c, x, y, fg, bg, 1);
}

static void draw_string_style(const char* str, uint32_t x, uint32_t y,
                              uint32_t fg, uint32_t bg, int bold) {
    if (!str) return;
    uint32_t cur_x = x, cur_y = y;
    while (*str) {
        if (*str == '\n') {
            cur_x = x;
            cur_y += FONT_HEIGHT + 2u;
        } else if (*str == '\r') {
            cur_x = x;
        } else if (*str == '\t') {
            cur_x += 4u * (FONT_WIDTH + 1u);
        } else {
            draw_char_style(*str, cur_x, cur_y, fg, bg, bold);
            cur_x += FONT_WIDTH + 1u;
        }
        ++str;
    }
}

void font_draw_string(const char* str, uint32_t x, uint32_t y,
                      uint32_t fg, uint32_t bg) {
    draw_string_style(str, x, y, fg, bg, 0);
}

void font_draw_string_bold(const char* str, uint32_t x, uint32_t y,
                           uint32_t fg, uint32_t bg) {
    draw_string_style(str, x, y, fg, bg, 1);
}
