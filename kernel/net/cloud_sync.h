#ifndef PHOXTRA_CLOUD_SYNC_H
#define PHOXTRA_CLOUD_SYNC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void cloud_sync_init(void);
void cloud_sync_trigger_e2ee_sync(void);
uint32_t cloud_sync_get_status(void);

#ifdef __cplusplus
}
#endif

#endif // PHOXTRA_CLOUD_SYNC_H
