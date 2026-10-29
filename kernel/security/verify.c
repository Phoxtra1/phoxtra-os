#include "verify.h"
#include "../log/logger.h"

void verify_init(void) {
    klog(LOG_WARNING, "Signature verification is unavailable: no trusted verification key store or signature scheme is configured");
}

uint32_t verify_file_signature(const char* file_path, const char* expected_hash) {
    (void)file_path;
    (void)expected_hash;
    klog(LOG_WARNING, "File signature verification unavailable; target remains unverified");
    return 0;
}

uint32_t verify_document_authenticity(const char* doc_id) {
    (void)doc_id;
    klog(LOG_WARNING, "Document authenticity verification unavailable; no blockchain or trusted document service is connected");
    return 0;
}
