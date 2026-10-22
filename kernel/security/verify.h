#ifndef PHOXTRA_VERIFY_H
#define PHOXTRA_VERIFY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void verify_init(void);
uint32_t verify_file_signature(const char* file_path, const char* expected_hash);
uint32_t verify_document_authenticity(const char* doc_id);

#ifdef __cplusplus
}
#endif

#endif // PHOXTRA_VERIFY_H
