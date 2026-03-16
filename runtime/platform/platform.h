#ifndef SILVEROS_PLATFORM_H
#define SILVEROS_PLATFORM_H

#include "../../include/types.h"

/* Monotonic time in milliseconds */
uint64_t platform_time_ms(void);

/* Console output */
void platform_console_write(const char *str);

/* Randomness */
uint32_t platform_random_u32(void);

/* Yield current thread */
void platform_yield(void);

#endif
