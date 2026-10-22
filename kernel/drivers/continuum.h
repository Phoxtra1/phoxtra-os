#ifndef PHOXTRA_CONTINUUM_H
#define PHOXTRA_CONTINUUM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CONTINUUM_MODE_MOBILE = 0,
    CONTINUUM_MODE_DESKTOP = 1
} continuum_mode_t;

void continuum_init(void);
continuum_mode_t continuum_get_mode(void);
void continuum_switch_mode(continuum_mode_t mode);

#ifdef __cplusplus
}
#endif

#endif // PHOXTRA_CONTINUUM_H
