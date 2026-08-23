#ifndef PHOXTRA_SHA256_H
#define PHOXTRA_SHA256_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t data[64];
    uint32_t datalen;
    uint64_t bitlen;
    uint32_t state[8];
} sha256_ctx_t;

void sha256_init(sha256_ctx_t* ctx);
void sha256_update(sha256_ctx_t* ctx, const uint8_t* data, size_t len);
void sha256_final(sha256_ctx_t* ctx, uint8_t hash[32]);
void sha256_hash(const uint8_t* data, size_t len, uint8_t hash[32]);

#ifdef __cplusplus
}
#endif

#endif
