#ifndef PHOXTRA_MBEDTLS_CONFIG_H
#define PHOXTRA_MBEDTLS_CONFIG_H

/* This client is freestanding in the guest, but X.509 certificate validity
 * checks MUST retain wall-clock support. The host test overrides time() to
 * exercise expiration; the guest time() syscall reads the CMOS RTC. */
#undef MBEDTLS_DEBUG_C
#undef MBEDTLS_SELF_TEST
#undef MBEDTLS_FS_IO
#undef MBEDTLS_NET_C
#undef MBEDTLS_TIMING_C
#undef MBEDTLS_PSA_ITS_FILE_C
#undef MBEDTLS_PSA_CRYPTO_STORAGE_C
#endif
