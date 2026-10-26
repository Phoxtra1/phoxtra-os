#ifndef PHOXTRA_PSA_CRYPTO_USER_CONFIG_H
#define PHOXTRA_PSA_CRYPTO_USER_CONFIG_H

/*
 * TF-PSA-Crypto requires external RNG selection in its own PSA configuration,
 * rather than only in Mbed TLS's user configuration. The platform callback
 * is implemented in libc/src/psa_rng.c and fails closed if OS entropy fails.
 */
#define MBEDTLS_PSA_CRYPTO_EXTERNAL_RNG

#endif
