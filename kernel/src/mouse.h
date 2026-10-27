#ifndef PHOXTRA_MOUSE_H
#define PHOXTRA_MOUSE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mouse_event {
    int32_t x;
    int32_t y;
    uint8_t buttons;
    uint8_t left_clicked;
    uint8_t right_clicked;
} mouse_event_t;

void mouse_init(void);
int mouse_is_available(void);
void mouse_get_position(int32_t* x, int32_t* y);
/* Returns 1 when a new motion/button event was available, otherwise 0. */
int mouse_poll_event(mouse_event_t* event);

#ifdef __cplusplus
}
#endif

#endif
