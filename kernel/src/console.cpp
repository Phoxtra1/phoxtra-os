#include "console.hpp"

namespace console
{
    volatile unsigned short* VGA_MEMORY =
        (unsigned short*)0xB8000;

    unsigned int cursor = 0;

    void clear()
    {
        for (unsigned int i = 0; i < 80 * 25; i++)
        {
            VGA_MEMORY[i] = 0x0720;
        }

        cursor = 0;
    }


    void write(const char* text)
    {
        while (*text)
        {
            VGA_MEMORY[cursor++] =
                0x0700 | *text;

            text++;
        }
    }
}
