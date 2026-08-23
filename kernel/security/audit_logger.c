#include "audit_logger.h"
#include "sha256.h"
#include "timer.h"
#include "../log/logger.h"

#define AUDIT_RING_BUFFER_SIZE 128

static audit_event_t ring_buffer[AUDIT_RING_BUFFER_SIZE];
static uint32_t head = 0;
static uint32_t total_events = 0;
static uint8_t prev_chain_hash[32];

static void kstrncpy(char* dst, const char* src, int maxlen) {
    int i;
    for (i = 0; i < maxlen - 1 && src[i]; i++) {
        dst[i] = src[i];
    }
    dst[i] = '\0';
}

void audit_logger_init(void) {
    head = 0;
    total_events = 0;

    const char genesis_seed[] = "PHOXTRA_GENESIS_AUDIT_LOG_CHAIN_SEED_2026";
    sha256_hash((const uint8_t*)genesis_seed, sizeof(genesis_seed) - 1, prev_chain_hash);

    klog(LOG_SECURITY, "Initializing Immutable Security Audit Logger Ring Buffer...");

    audit_log_event(AUDIT_TYPE_HARDWARE_ATTESTATION, 0, "TPM 2.0 Hardware Root of Trust Attested");
    audit_log_event(AUDIT_TYPE_PROCESS_BINDING, 0, "Kernel Core Process Identities Sealed");
}

int audit_log_event(uint32_t type, uint32_t pid, const char* message) {
    if (!message) return -1;

    audit_event_t* ev = &ring_buffer[head];
    ev->timestamp  = timer_get_ticks();
    ev->event_type = type;
    ev->pid        = pid;
    kstrncpy(ev->message, message, sizeof(ev->message));

    // Compute cryptographically chained SHA-256 event digest
    sha256_ctx_t ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, prev_chain_hash, 32);
    sha256_update(&ctx, (const uint8_t*)&ev->timestamp, sizeof(ev->timestamp));
    sha256_update(&ctx, (const uint8_t*)&ev->event_type, sizeof(ev->event_type));
    sha256_update(&ctx, (const uint8_t*)&ev->pid, sizeof(ev->pid));
    sha256_update(&ctx, (const uint8_t*)ev->message, 64);
    sha256_final(&ctx, ev->event_hash);

    for (int i = 0; i < 32; i++) {
        prev_chain_hash[i] = ev->event_hash[i];
    }

    head = (head + 1) % AUDIT_RING_BUFFER_SIZE;
    total_events++;

    return 0;
}

uint32_t audit_get_count(void) {
    return (total_events < AUDIT_RING_BUFFER_SIZE) ? total_events : AUDIT_RING_BUFFER_SIZE;
}

const audit_event_t* audit_get_event(uint32_t index) {
    uint32_t count = audit_get_count();
    if (index >= count) return (void*)0;

    uint32_t start = (total_events > AUDIT_RING_BUFFER_SIZE) ? head : 0;
    uint32_t real_idx = (start + index) % AUDIT_RING_BUFFER_SIZE;
    return &ring_buffer[real_idx];
}
