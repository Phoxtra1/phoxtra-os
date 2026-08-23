#ifndef PHOXTRA_WM_H
#define PHOXTRA_WM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void wm_init(void);
void wm_start_desktop(void);
uint32_t wm_get_window_count(void);

#ifdef __cplusplus
}
#endif

#endif
