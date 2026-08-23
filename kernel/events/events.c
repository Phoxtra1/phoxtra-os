#include "events.h"
#include "../log/logger.h"

/* Subscriber table: each event type can have up to MAX_EVENT_SUBSCRIBERS handlers */
static event_handler_fn subscribers[MAX_EVENT_TYPES][MAX_EVENT_SUBSCRIBERS];

void events_init(void) {
    for (int t = 0; t < MAX_EVENT_TYPES; t++)
        for (int s = 0; s < MAX_EVENT_SUBSCRIBERS; s++)
            subscribers[t][s] = (void*)0;

    klog(LOG_KERNEL, "Kernel Event System initialized");
}

int events_subscribe(event_type_t type, event_handler_fn handler) {
    if ((int)type <= 0 || (int)type >= MAX_EVENT_TYPES || !handler) return -1;

    for (int s = 0; s < MAX_EVENT_SUBSCRIBERS; s++) {
        if (!subscribers[(int)type][s]) {
            subscribers[(int)type][s] = handler;
            return 0;
        }
    }
    klog(LOG_WARNING, "Event bus: subscriber table full for event type");
    return -1;
}

void events_unsubscribe(event_type_t type, event_handler_fn handler) {
    if ((int)type <= 0 || (int)type >= MAX_EVENT_TYPES || !handler) return;

    for (int s = 0; s < MAX_EVENT_SUBSCRIBERS; s++) {
        if (subscribers[(int)type][s] == handler) {
            subscribers[(int)type][s] = (void*)0;
            return;
        }
    }
}

void events_publish(event_type_t type, uint64_t d0, uint64_t d1, uint64_t d2) {
    if ((int)type <= 0 || (int)type >= MAX_EVENT_TYPES) return;

    kernel_event_t evt = { type, d0, d1, d2 };

    for (int s = 0; s < MAX_EVENT_SUBSCRIBERS; s++) {
        if (subscribers[(int)type][s]) {
            subscribers[(int)type][s](&evt);
        }
    }
}
