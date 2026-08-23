#ifndef PHOXTRA_INSTALLER_H
#define PHOXTRA_INSTALLER_H

#include <stdint.h>

typedef enum installer_step {
    INSTALLER_WELCOME = 0,
    INSTALLER_DISK_SELECT = 1,
    INSTALLER_PARTITION_FORMAT = 2,
    INSTALLER_COPYING_FILES = 3,
    INSTALLER_COMPLETE = 4
} installer_step_t;

#ifdef __cplusplus
extern "C" {
#endif

void installer_init(void);
void installer_start(void);
void installer_next_step(void);
installer_step_t installer_get_step(void);

#ifdef __cplusplus
}
#endif

#endif
