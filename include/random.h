#ifndef SILVEROS_RANDOM_H
#define SILVEROS_RANDOM_H

#include "types.h"

void get_random_bytes(void *buf, size_t len);
uint32_t get_random_u32(void);

#endif
