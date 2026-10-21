#include "console.hpp"
#include "../drivers/framebuffer.h"
#include "../ui/font.h"
#include <stdint.h>

namespace console {
    volatile unsigned short* VGA_MEMORY = (unsigned short*)0xB8000;
    unsigned int row = 0;
    unsigned int column = 0;
    static char cells[25][80];

    static void draw_cell(unsigned int r, unsigned int c) {
        if (!framebuffer_is_available() || r >= 25 || c >= 80) return;
        char ch = cells[r][c];
        if (ch != ' ') font_draw_char(ch, c * 8, r * 16, 0x00F8FAFC, 0x00000000);
    }

    static void redraw_framebuffer_console(void) {
        if (!framebuffer_is_available()) return;
        fb_clear(0x00000000);
        for (unsigned int r = 0; r < 25; r++)
            for (unsigned int c = 0; c < 80; c++)
                if (cells[r][c] != ' ') draw_cell(r, c);
        fb_swap_buffers();
    }

    static void scroll(void) {
        for (unsigned int r = 0; r < 24; r++)
            for (unsigned int c = 0; c < 80; c++) cells[r][c] = cells[r + 1][c];
        for (unsigned int c = 0; c < 80; c++) cells[24][c] = ' ';
        row = 24;
        column = 0;
        if (framebuffer_is_available()) redraw_framebuffer_console();
        else {
            for (unsigned int i = 0; i < 24 * 80; i++) VGA_MEMORY[i] = VGA_MEMORY[i + 80];
            for (unsigned int i = 24 * 80; i < 25 * 80; i++) VGA_MEMORY[i] = 0x0720;
        }
    }

    void clear() {
        row = 0;
        column = 0;
        for (unsigned int r = 0; r < 25; r++)
            for (unsigned int c = 0; c < 80; c++) cells[r][c] = ' ';
        for (unsigned int i = 0; i < 80 * 25; i++) VGA_MEMORY[i] = 0x0720;
        if (framebuffer_is_available()) {
            fb_clear(0x00000000);
            fb_swap_buffers();
        }
    }

    static void put_char_no_present(char ch) {
        if (ch == '\n') {
            row++;
            column = 0;
            if (row >= 25) scroll();
            return;
        }
        if (ch == '\b') {
            if (column > 0) column--;
            else if (row > 0) { row--; column = 79; }
            cells[row][column] = ' ';
            VGA_MEMORY[row * 80 + column] = 0x0720;
            draw_cell(row, column);
            return;
        }
        cells[row][column] = ch;
        VGA_MEMORY[row * 80 + column] = (unsigned short)(0x0700 | (uint8_t)ch);
        draw_cell(row, column);
        column++;
        if (column >= 80) { column = 0; row++; }
        if (row >= 25) scroll();
    }

    void put_char(char ch) {
        put_char_no_present(ch);
        if (framebuffer_is_available()) fb_swap_buffers();
    }

    void write(const char* text) {
        if (!text) return;
        while (*text) put_char_no_present(*text++);
        if (framebuffer_is_available()) fb_swap_buffers();
    }
}

extern "C" {
    void console_clear(void) { console::clear(); }
    void console_write(const char* text) { console::write(text); }
    void console_put_char(char ch) { console::put_char(ch); }
}
