#ifndef PHOXTRA_TPM2_H
#define PHOXTRA_TPM2_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TPM2_PCR_BOOT_LOADER    0
#define TPM2_PCR_KERNEL_CORE    1
#define TPM2_PCR_SYS_CONFIG     2
#define TPM2_PCR_PROCESS_TABLE  3

typedef struct {
    uint8_t  initialized;
    uint8_t  hardware_attested;
    uint32_t active_pcrs;
    uint8_t  pcr_digests[24][32];
    uint8_t  endorsement_key[32];
    char     vendor_id[32];
} tpm2_dev_t;

void tpm2_init(void);
int tpm2_is_present(void);
tpm2_dev_t* tpm2_get_device(void);
int tpm2_pcr_read(uint32_t pcr_index, uint8_t digest_out[32]);
int tpm2_pcr_extend(uint32_t pcr_index, const uint8_t digest[32]);
void tpm2_get_attestation_quote(uint8_t quote_out[64]);

#ifdef __cplusplus
}
#endif

#endif
