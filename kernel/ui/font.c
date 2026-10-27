#include "font.h"
#include "font_carlito_aa_data.h"
#include "../drivers/framebuffer.h"
#include "../log/logger.h"

static uint8_t glyph_data[PHOXTRA_AA_GLYPH_COUNT][PHOXTRA_AA_GLYPH_BYTES];
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
    uint32_t written = 0, accumulator = 0, bits = 0;
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

int font_is_ready(void) { return font_ready; }

void font_init(void) {
    uint32_t decoded = decode_base64(phoxtra_carlito_aa_b64,
                                     &glyph_data[0][0], sizeof(glyph_data));
    font_ready = decoded == sizeof(glyph_data);
    klog(font_ready ? LOG_INFO : LOG_ERROR,
         font_ready ? "Carlito proportional antialiased font initialized"
                    : "Carlito font bitmap invalid; text renderer disabled");
}

static uint8_t glyph_coverage(uint32_t index, uint32_t x, uint32_t y) {
    if (x >= PHOXTRA_AA_GLYPH_WIDTH || y >= PHOXTRA_AA_GLYPH_HEIGHT) return 0;
    uint32_t pixel = y * PHOXTRA_AA_GLYPH_WIDTH + x;
    uint8_t packed = glyph_data[index][pixel / 4u];
    return (uint8_t)((packed >> (6u - 2u * (pixel % 4u))) & 0x03u);
}

static uint32_t blend_channel(uint32_t foreground, uint32_t background, uint8_t coverage) {
    return (foreground * coverage + background * (3u - coverage) + 1u) / 3u;
}

static uint32_t blend_coverage(uint32_t foreground, uint32_t background, uint8_t coverage) {
    uint32_t r = blend_channel((foreground >> 16) & 0xFFu, (background >> 16) & 0xFFu, coverage);
    uint32_t g = blend_channel((foreground >> 8) & 0xFFu, (background >> 8) & 0xFFu, coverage);
    uint32_t b = blend_channel(foreground & 0xFFu, background & 0xFFu, coverage);
    return (r << 16) | (g << 8) | b;
}

static void draw_char_style(char c, uint32_t x, uint32_t y,
                            uint32_t fg, uint32_t bg, int bold) {
    uint8_t code = (uint8_t)c;
    if (code < 32 || code > 126) code = '?';
    uint32_t index = (uint32_t)code - 32u;

    for (uint32_t row = 0; row < PHOXTRA_AA_GLYPH_HEIGHT; ++row) {
        for (uint32_t col = 0; col < PHOXTRA_AA_GLYPH_WIDTH; ++col) {
            uint8_t coverage = font_ready ? glyph_coverage(index, col, row) : 0;
            if (bold && col > 0) {
                uint8_t left = font_ready ? glyph_coverage(index, col - 1u, row) : 0;
                if (left > coverage) coverage = left;
            }

            uint32_t px = x + col, py = y + row;
            if (px < x || py < y) continue;
            if (coverage) {
                uint32_t background = bg ? bg : fb_get_back_pixel(px, py);
                fb_put_pixel(px, py, blend_coverage(fg, background, coverage));
            } else if (bg) {
                fb_put_pixel(px, py, bg);
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

static uint32_t advance_for(char c) {
    uint8_t code = (uint8_t)c;
    if (code < 32 || code > 126) code = '?';
    return phoxtra_carlito_advance[(uint32_t)code - 32u];
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
            cur_x += 4u * (FONT_AVG_WIDTH + 1u);
        } else {
            draw_char_style(*str, cur_x, cur_y, fg, bg, bold);
            cur_x += advance_for(*str);
        }
        ++str;
    }
}

void font_draw_string(const char* str, uint32_t x, uint32_t y, uint32_t fg, uint32_t bg) {
    draw_string_style(str, x, y, fg, bg, 0);
}

void font_draw_string_bold(const char* str, uint32_t x, uint32_t y, uint32_t fg, uint32_t bg) {
    draw_string_style(str, x, y, fg, bg, 1);
}
