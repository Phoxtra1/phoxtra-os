#include "stress.h"
#include "../log/logger.h"
static uint32_t active_stress_tasks;
void stress_init(void) {
    active_stress_tasks = 0;
    klog(LOG_WARNING, "Scheduler stress tests are unavailable until process registration/context switching is integrated");
}
int stress_test_multitasking(uint32_t num_tasks) {
    (void)num_tasks;
    active_stress_tasks = 0;
    klog(LOG_WARNING, "Stress test not run; process scheduling and task-return handling are not validated");
    return -2;
}
uint32_t stress_get_active_task_count(void) { return active_stress_tasks; }
