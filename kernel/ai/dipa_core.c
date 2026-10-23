#include "dipa_core.h"
#include "../log/logger.h"
static uint32_t dipa_active = 0;
void dipa_ai_init(void) {
    dipa_active = 0;
    klog(LOG_WARNING, "Dipa inference backend is not connected; kernel stub does not execute AI queries");
}
void dipa_ai_process_query(const char* query, char* out, uint32_t max_len) {
    (void)query;
    if (!out || !max_len) return;
    const char* message = "Dipa AI is unavailable: no inference model/API is connected.";
    uint32_t i = 0;
    while (message[i] && i + 1 < max_len) { out[i] = message[i]; ++i; }
    out[i] = '\0';
}
uint32_t dipa_ai_get_status(void) { return dipa_active; }
