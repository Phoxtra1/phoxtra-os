#ifndef PHOXTRA_EVENTS_H
#define PHOXTRA_EVENTS_H

#include <stdint.h>

#define MAX_EVENT_SUBSCRIBERS 16
#define MAX_EVENT_TYPES       32

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    EVENT_NONE          = 0,
    EVENT_TIMER_TICK    = 1,
    EVENT_KEY_PRESSED   = 2,
    EVENT_PROCESS_SPAWN = 3,
    EVENT_PROCESS_KILL  = 4,
    EVENT_MEMORY_ALLOC  = 5,
    EVENT_MEMORY_FREE   = 6,
    EVENT_LOG_MESSAGE   = 7,
    EVENT_SECURITY      = 8,
    EVENT_SCHEDULER     = 9,
    EVENT_SYSCALL       = 10,
    EVENT_MAX           = MAX_EVENT_TYPES
} event_type_t;

typedef struct {
    event_type_t type;
    uint64_t     data0;
    uint64_t     data1;
    uint64_t     data2;
} kernel_event_t;

typedef void (*event_handler_fn)(const kernel_event_t* event);

void events_init(void);
int  events_subscribe(event_type_t type, event_handler_fn handler);
void events_unsubscribe(event_type_t type, event_handler_fn handler);
void events_publish(event_type_t type, uint64_t d0, uint64_t d1, uint64_t d2);

#ifdef __cplusplus
}
#endif

#endif
