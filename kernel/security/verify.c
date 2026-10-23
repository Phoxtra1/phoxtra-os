#include "verify.h"
#include "../log/logger.h"
void verify_init(void) {
    klog(LOG_WARNING, "Phoxtra Verify is not operational: trusted key store and document-verification backend are not connected");
}
uint32_t verify_file_signature(const char* file_path, const char* expected_hash) {
    if (!file_path || !expected_hash) return 0;
    klog(LOG_WARNING, "File signature verification unavailable; file is not marked authentic");
    return 0;
}
uint32_t verify_document_authenticity(const char* doc_id) {
    if (!doc_id) return 0;
    klog(LOG_WARNING, "Document authenticity verification unavailable; document is not marked authentic");
    return 0;
}
