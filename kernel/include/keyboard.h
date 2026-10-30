#ifndef PHOXTRA_KEYBOARD_H
#define PHOXTRA_KEYBOARD_H

#include <stdint.h>

void keyboard_init(void);
char keyboard_get_char(void);
#define KEYBOARD_MOD_CONTROL 0x01u
#define KEYBOARD_MOD_SHIFT 0x02u
int keyboard_poll_char(char* out);
int keyboard_poll_event(char* out, uint8_t* modifiers);
int keyboard_control_pressed(void);

#endif
