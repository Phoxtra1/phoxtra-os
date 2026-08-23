#ifndef PHOXTRA_DIPA_NL_H
#define PHOXTRA_DIPA_NL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void dipa_nl_init(void);
void dipa_nl_parse_voice_command(const char* transcript);
void dipa_nl_optimize_system(void);

#ifdef __cplusplus
}
#endif

#endif // PHOXTRA_DIPA_NL_H
