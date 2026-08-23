#include "dipa_core.h"
#include "../log/logger.h"

static uint32_t dipa_active = 0;

void dipa_ai_init(void) {
    dipa_active = 1;
    klog(LOG_INFO, "Initializing Dipa AI Core OS Daemon (Dipa Engine v2.0)...");
}

void dipa_ai_process_query(const char* query, char* out_response, uint32_t max_len) {
    if (!query || !out_response || max_len == 0) return;

    // Direct Dipa AI Query Parser
    klog(LOG_INFO, "Dipa AI Processing Kernel Query...");
    
    const char* prefix = "[Dipa AI] System analyzed your request. Execution ready: ";
    uint32_t p = 0;
    while (prefix[p] && p < max_len - 1) {
        out_response[p] = prefix[p];
        p++;
    }
    uint32_t q = 0;
    while (query[q] && p < max_len - 1) {
        out_response[p++] = query[q++];
    }
    out_response[p] = '\0';
}

uint32_t dipa_ai_get_status(void) {
    return dipa_active;
}
