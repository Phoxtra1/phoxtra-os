#ifndef PHOXTRA_PSA_CRYPTO_USER_CONFIG_H
#define PHOXTRA_PSA_CRYPTO_USER_CONFIG_H

/*
 * TF-PSA-Crypto requires external RNG selection in its own PSA configuration,
 * rather than only in Mbed TLS's user configuration. The platform callback
 * is implemented in libc/src/psa_rng.c and fails closed if OS entropy fails.
 */
#define MBEDTLS_PSA_CRYPTO_EXTERNAL_RNG

/* Phoxtra currently has no persistent PSA storage backend. Do not link the
 * upstream file-backed ITS simulator into the freestanding guest image. */
#undef MBEDTLS_PSA_ITS_FILE_C
#undef MBEDTLS_PSA_CRYPTO_STORAGE_C

#endif
