#include "tpm2.h"
#include "../log/logger.h"
static tpm2_dev_t tpm2_dev;
void tpm2_init(void) {
    uint8_t* bytes = (uint8_t*)&tpm2_dev;
    for (size_t i = 0; i < sizeof(tpm2_dev); ++i) bytes[i] = 0;
    klog(LOG_WARNING, "No TPM 2.0 hardware driver is connected; hardware attestation and PCR operations are unavailable");
}
int tpm2_is_present(void) { return 0; }
tpm2_dev_t* tpm2_get_device(void) { return &tpm2_dev; }
int tpm2_pcr_read(uint32_t pcr_index, uint8_t digest_out[32]) {
    (void)pcr_index; (void)digest_out;
    return -2;
}
int tpm2_pcr_extend(uint32_t pcr_index, const uint8_t digest[32]) {
    (void)pcr_index; (void)digest;
    return -2;
}
void tpm2_get_attestation_quote(uint8_t quote_out[64]) {
    if (!quote_out) return;
    for (uint32_t i = 0; i < 64; ++i) quote_out[i] = 0;
}
