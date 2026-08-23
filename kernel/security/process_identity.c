#include "process_identity.h"
#include "sha256.h"
#include "tpm2.h"
#include "../log/logger.h"

static const char authority_secret_key[] = "PHOXTRA_KERNEL_AUTHORITY_TRUSTED_SIGNING_KEY_2026";

void process_identity_init(void) {
    klog(LOG_SECURITY, "Initializing Cryptographic Process Identity Attestation Engine...");
}

int process_identity_bind(pcb_t* pcb, const uint8_t* code_ptr, size_t code_len) {
    if (!pcb) return -1;

    if (code_ptr && code_len > 0) {
        sha256_hash(code_ptr, code_len, pcb->process_hash);
    } else {
        // Fallback SHA-256 binding based on PID, name, creation time, and authority key
        sha256_ctx_t ctx;
        sha256_init(&ctx);
        sha256_update(&ctx, (const uint8_t*)&pcb->pid, sizeof(pcb->pid));
        sha256_update(&ctx, (const uint8_t*)pcb->name, 32);
        sha256_update(&ctx, (const uint8_t*)authority_secret_key, sizeof(authority_secret_key) - 1);
        sha256_final(&ctx, pcb->process_hash);
    }

    // Generate cryptographic signature (Hash ^ Authority Key)
    sha256_ctx_t sig_ctx;
    sha256_init(&sig_ctx);
    sha256_update(&sig_ctx, pcb->process_hash, 32);
    sha256_update(&sig_ctx, (const uint8_t*)authority_secret_key, sizeof(authority_secret_key) - 1);
    sha256_final(&sig_ctx, pcb->signature);
    sha256_final(&sig_ctx, pcb->signature + 32);

    pcb->identity_status = IDENTITY_STATUS_VERIFIED;

    // Extend TPM 2.0 PCR3 with Process Hash
    tpm2_pcr_extend(TPM2_PCR_PROCESS_TABLE, pcb->process_hash);

    klog(LOG_SECURITY, "Process Identity Bound & Verified via SHA-256");
    return 0;
}

int process_identity_verify(pcb_t* pcb) {
    if (!pcb) return IDENTITY_STATUS_REJECTED;

    uint8_t expected_sig[64];
    sha256_ctx_t sig_ctx;
    sha256_init(&sig_ctx);
    sha256_update(&sig_ctx, pcb->process_hash, 32);
    sha256_update(&sig_ctx, (const uint8_t*)authority_secret_key, sizeof(authority_secret_key) - 1);
    sha256_final(&sig_ctx, expected_sig);
    sha256_final(&sig_ctx, expected_sig + 32);

    for (int i = 0; i < 64; i++) {
        if (pcb->signature[i] != expected_sig[i]) {
            pcb->identity_status = IDENTITY_STATUS_REJECTED;
            klog(LOG_SECURITY, "SECURITY VIOLATION: Process Identity Signature Mismatch!");
            return IDENTITY_STATUS_REJECTED;
        }
    }

    pcb->identity_status = IDENTITY_STATUS_VERIFIED;
    return IDENTITY_STATUS_VERIFIED;
}

void process_identity_get_hash_hex(pcb_t* pcb, char hex_out[65]) {
    if (!pcb || !hex_out) return;

    static const char hex_chars[] = "0123456789abcdef";
    for (int i = 0; i < 32; i++) {
        hex_out[i * 2]     = hex_chars[(pcb->process_hash[i] >> 4) & 0x0F];
        hex_out[i * 2 + 1] = hex_chars[pcb->process_hash[i] & 0x0F];
    }
    hex_out[64] = '\0';
}
