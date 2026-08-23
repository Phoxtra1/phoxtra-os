#ifndef PHOXTRA_CONSOLE_HPP
#define PHOXTRA_CONSOLE_HPP

namespace console
{
    void clear();
    void write(const char* text);
    void put_char(char c);
}

#ifdef __cplusplus
extern "C" {
#endif
void console_clear(void);
void console_write(const char* text);
void console_put_char(char c);
#ifdef __cplusplus
}
#endif

#endif
