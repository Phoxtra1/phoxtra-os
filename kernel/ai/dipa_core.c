#include "dipa_core.h"
#include "../log/logger.h"

static uint32_t dipa_active = 0;

void dipa_ai_init(void) {
    dipa_active = 0;
    klog(LOG_WARNING, "Dipa AI engine is not integrated; query execution is unavailable");
}

void dipa_ai_process_query(const char* query, char* out_response, uint32_t max_len) {
    (void)query;
    if (!out_response || max_len == 0) return;
    const char* message = "Dipa AI service unavailable; query was not processed.";
    uint32_t i = 0;
    while (message[i] && i + 1u < max_len) {
        out_response[i] = message[i];
        ++i;
    }
    out_response[i] = '\0';
}

uint32_t dipa_ai_get_status(void) {
    return dipa_active;
}
