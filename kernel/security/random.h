#ifndef PHOXTRA_SECURITY_RANDOM_H
#define PHOXTRA_SECURITY_RANDOM_H
#include <stddef.h>
#include <stdint.h>
int phoxtra_hw_random(uint8_t* output, size_t length);
#endif
