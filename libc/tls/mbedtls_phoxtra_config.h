#ifndef PHOXTRA_MBEDTLS_CONFIG_H
#define PHOXTRA_MBEDTLS_CONFIG_H

/* This guest/host client does not use diagnostic, self-test, filesystem,
 * socket, timing or wall-clock facilities from the hosted default profile. */
#undef MBEDTLS_DEBUG_C
#undef MBEDTLS_SELF_TEST
#undef MBEDTLS_FS_IO
#undef MBEDTLS_NET_C
#undef MBEDTLS_TIMING_C
#undef MBEDTLS_HAVE_TIME
#undef MBEDTLS_HAVE_TIME_DATE
#undef MBEDTLS_PSA_ITS_FILE_C
#undef MBEDTLS_PSA_CRYPTO_STORAGE_C
#endif
