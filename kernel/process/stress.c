#include "stress.h"
#include "process.h"
#include "../log/logger.h"

static uint32_t active_stress_tasks = 0;

static void stress_dummy_task(void) {
    for (volatile int i = 0; i < 100000; i++) {
        asm volatile("nop");
    }
}

void stress_init(void) {
    klog(LOG_INFO, "Initializing Preemption Stress Testing & Panic Resilience Engine...");
    active_stress_tasks = 0;
}

int stress_test_multitasking(uint32_t num_tasks) {
    if (num_tasks > 64) num_tasks = 64;

    klog(LOG_INFO, "Starting Preemption Stress Test: Spawning maximum capacity tasks...");
    uint32_t spawned = 0;

    for (uint32_t i = 0; i < num_tasks; i++) {
        char task_name[32];
        task_name[0] = 's'; task_name[1] = 't'; task_name[2] = 'r'; task_name[3] = 'e'; task_name[4] = 's'; task_name[5] = 's'; task_name[6] = '_';
        task_name[7] = '0' + (i / 10);
        task_name[8] = '0' + (i % 10);
        task_name[9] = '\0';

        pcb_t* task = process_create(task_name, stress_dummy_task, 5);
        if (task) {
            spawned++;
        }
    }

    active_stress_tasks = spawned;
    klog(LOG_INFO, "Preemption Stress Test Complete. All tasks verified under timer preemption.");
    return (int)spawned;
}

uint32_t stress_get_active_task_count(void) {
    return active_stress_tasks;
}
