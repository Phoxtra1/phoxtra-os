#ifndef PHOXTRA_GUEST_HTTPS_IMAGE_H
#define PHOXTRA_GUEST_HTTPS_IMAGE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
const uint8_t* phoxtra_guest_https_elf_bytes(void);
size_t phoxtra_guest_https_elf_size(void);
void guest_https_smoke_start(void);
#ifdef __cplusplus
}
#endif
#endif
