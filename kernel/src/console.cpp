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
            row = 24;
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
