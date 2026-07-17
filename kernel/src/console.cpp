#include "console.hpp"

namespace console
{
    static volatile unsigned short* const VGA_MEMORY =
        (unsigned short*)0xB8000;

    static unsigned int cursor = 0;

    static const unsigned char COLOR = 0x07;


    void clear()
    {
        for (unsigned int i = 0; i < 80 * 25; i++)
        {
            VGA_MEMORY[i] =
                ((unsigned short)COLOR << 8) | ' ';
        }

        cursor = 0;
    }


    void write(const char* text)
    {
        while (*text)
        {
            if (*text == '\n')
            {
                cursor += 80 - (cursor % 80);
            }
            else
            {
                VGA_MEMORY[cursor] =
                    ((unsigned short)COLOR << 8) | *text;

                cursor++;
            }

            text++;
        }
    }
}
