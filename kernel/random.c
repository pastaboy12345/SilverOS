#include "../include/random.h"

static uint32_t state = 0x12345678;

static uint64_t rdtsc(void) {
    uint32_t low, high;
    __asm__ volatile("rdtsc" : "=a"(low), "=d"(high));
    return ((uint64_t)high << 32) | low;
}

void get_random_bytes(void *buf, size_t len) {
    uint8_t *p = (uint8_t *)buf;
    if (state == 0x12345678) {
        state ^= (uint32_t)rdtsc();
    }

    for (size_t i = 0; i < len; i++) {
        /* Xorshift32 */
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        p[i] = (uint8_t)(state & 0xFF);
    }
}

uint32_t get_random_u32(void) {
    uint32_t val;
    get_random_bytes(&val, sizeof(val));
    return val;
}
