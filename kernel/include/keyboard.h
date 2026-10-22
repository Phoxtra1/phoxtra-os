#ifndef PHOXTRA_KEYBOARD_H
#define PHOXTRA_KEYBOARD_H

#include <stdint.h>

void keyboard_init(void);
char keyboard_get_char(void);
int keyboard_poll_char(char* out);
int keyboard_control_pressed(void);

#endif
