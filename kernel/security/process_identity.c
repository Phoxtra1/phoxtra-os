#include "process_identity.h"
#include "sha256.h"
#include "../log/logger.h"
void process_identity_init(void) {
    klog(LOG_WARNING, "Process code fingerprinting is available; trusted signatures and hardware attestation are not implemented");
}
int process_identity_bind(pcb_t* pcb, const uint8_t* code_ptr, size_t code_len) {
    if (!pcb) return -1;
    if (code_ptr && code_len) {
        sha256_hash(code_ptr, code_len, pcb->process_hash);
    } else {
        sha256_ctx_t ctx;
        sha256_init(&ctx);
        sha256_update(&ctx, (const uint8_t*)&pcb->pid, sizeof(pcb->pid));
        sha256_update(&ctx, (const uint8_t*)pcb->name, sizeof(pcb->name));
        sha256_final(&ctx, pcb->process_hash);
    }
    for (uint32_t i = 0; i < sizeof(pcb->signature); ++i) pcb->signature[i] = 0;
    pcb->identity_status = IDENTITY_STATUS_UNVERIFIED;
    klog(LOG_INFO, "SHA-256 process fingerprint recorded; no signature or attestation is claimed");
    return 0;
}
int process_identity_verify(pcb_t* pcb) {
    if (!pcb) return IDENTITY_STATUS_REJECTED;
    pcb->identity_status = IDENTITY_STATUS_UNVERIFIED;
    klog(LOG_WARNING, "Process signature verification unavailable: no trusted key store or signature scheme is configured");
    return IDENTITY_STATUS_UNVERIFIED;
}
void process_identity_get_hash_hex(pcb_t* pcb, char hex_out[65]) {
    if (!pcb || !hex_out) return;
    static const char hex[] = "0123456789abcdef";
    for (int i = 0; i < 32; ++i) {
        hex_out[i*2] = hex[(pcb->process_hash[i] >> 4) & 0x0f];
        hex_out[i*2+1] = hex[pcb->process_hash[i] & 0x0f];
    }
    hex_out[64] = '\0';
}
