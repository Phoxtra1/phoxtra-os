#ifndef PHOXTRA_BOX_H
#define PHOXTRA_BOX_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void phoxtra_box_init(void);
uint32_t phoxtra_box_run_apk(const char* apk_path);
uint32_t phoxtra_box_run_exe(const char* exe_path);

#ifdef __cplusplus
}
#endif

#endif // PHOXTRA_BOX_H
