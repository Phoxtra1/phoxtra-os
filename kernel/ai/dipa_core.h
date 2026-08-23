#ifndef PHOXTRA_DIPA_CORE_H
#define PHOXTRA_DIPA_CORE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void dipa_ai_init(void);
void dipa_ai_process_query(const char* query, char* out_response, uint32_t max_len);
uint32_t dipa_ai_get_status(void);

#ifdef __cplusplus
}
#endif

#endif // PHOXTRA_DIPA_CORE_H
