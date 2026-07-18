#include "serial.hpp"

namespace
{
    inline void outb(unsigned short port, unsigned char value)
    {
        asm volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
    }

    inline unsigned char inb(unsigned short port)
    {
        unsigned char value;
        asm volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
        return value;
    }
}

namespace serial
{

void init()
{
    outb(0x3F8 + 1, 0x00);
    outb(0x3F8 + 3, 0x80);
    outb(0x3F8 + 0, 0x03);
    outb(0x3F8 + 1, 0x00);
    outb(0x3F8 + 3, 0x03);
    outb(0x3F8 + 2, 0xC7);
    outb(0x3F8 + 4, 0x0B);
}

static bool transmit_empty()
{
    return inb(0x3F8 + 5) & 0x20;
}

void write(const char* str)
{
    while (*str)
    {
        while (!transmit_empty()) {}

        outb(0x3F8, *str++);
    }
}

}
