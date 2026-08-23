#ifndef PHOXTRA_SCHEDULER_H
#define PHOXTRA_SCHEDULER_H

#include "../process/pcb.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Scheduling algorithms — extensible for future implementations */
typedef enum {
    SCHED_ROUND_ROBIN,
    SCHED_PRIORITY,
    SCHED_FAIR,
    SCHED_REALTIME,
    SCHED_SECURITY_AWARE
} sched_algorithm_t;

void    scheduler_init(void);
void    scheduler_add_process(pcb_t* pcb);
void    scheduler_remove_process(uint32_t pid);
void    scheduler_tick(void);        /* Called on every timer IRQ */
void    scheduler_yield(void);       /* Current process voluntarily yields */
pcb_t*  scheduler_current(void);    /* Returns the currently running process */
void    scheduler_set_algorithm(sched_algorithm_t algo);
uint64_t scheduler_get_uptime_ticks(void);

#ifdef __cplusplus
}
#endif

#endif
