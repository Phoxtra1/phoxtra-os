#include "console.hpp"

namespace console
{
    volatile unsigned short* VGA_MEMORY =
        (unsigned short*)0xB8000;

    unsigned int row = 0;
    unsigned int column = 0;

    void clear()
    {
        for (unsigned int i = 0; i < 80 * 25; i++)
        {
            VGA_MEMORY[i] = 0x0720;
        }

        row = 0;
        column = 0;
    }

    void put_char(char c)
    {
        if (c == '\n')
        {
            row++;
            column = 0;
            if (row >= 25)
            {
                // Scroll up
                for (unsigned int i = 0; i < 24 * 80; i++)
                {
                    VGA_MEMORY[i] = VGA_MEMORY[i + 80];
                }
                for (unsigned int i = 24 * 80; i < 25 * 80; i++)
                {
                    VGA_MEMORY[i] = 0x0720;
                }
                row = 24;
            }
            return;
        }

        if (c == '\b')
        {
            if (column > 0)
            {
                column--;
            }
            else if (row > 0)
            {
                row--;
                column = 79;
            }
            VGA_MEMORY[row * 80 + column] = 0x0720;
            return;
        }

        VGA_MEMORY[row * 80 + column] =
            0x0700 | c;

        column++;

        if (column >= 80)
        {
            column = 0;
            row++;
        }

        if (row >= 25)
        {
            // Scroll up
            for (unsigned int i = 0; i < 24 * 80; i++)
            {
                VGA_MEMORY[i] = VGA_MEMORY[i + 80];
            }
            for (unsigned int i = 24 * 80; i < 25 * 80; i++)
            {
                VGA_MEMORY[i] = 0x0720;
            }
            row = 24;
            column = 0;
        }
    }

    void write(const char* text)
    {
        while (*text)
        {
            put_char(*text);
            text++;
        }
    }
}

extern "C" {
    void console_clear(void) {
        console::clear();
    }
    void console_write(const char* text) {
        console::write(text);
    }
    void console_put_char(char c) {
        console::put_char(c);
    }
}
