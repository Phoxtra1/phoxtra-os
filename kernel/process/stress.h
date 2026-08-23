#ifndef PHOXTRA_STRESS_H
#define PHOXTRA_STRESS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void stress_init(void);
int stress_test_multitasking(uint32_t num_tasks);
uint32_t stress_get_active_task_count(void);

#ifdef __cplusplus
}
#endif

#endif
