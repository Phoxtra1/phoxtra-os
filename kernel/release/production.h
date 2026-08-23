#ifndef PHOXTRA_PRODUCTION_H
#define PHOXTRA_PRODUCTION_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void production_init(void);
const char* production_get_version(void);
const char* production_get_lts_status(void);
int production_run_baremetal_certification(void);

#ifdef __cplusplus
}
#endif

#endif
