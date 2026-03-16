#include "platform.h"
#include "../../include/timer.h"
#include "../../include/console.h"
#include "../../include/random.h"
#include "../../include/io.h"

uint64_t platform_time_ms(void) {
    /* timer_ticks increments at 1000 Hz if initialized with 1000 */
    return timer_get_ticks();
}

void platform_console_write(const char *str) {
    console_puts(str);
}

uint32_t platform_random_u32(void) {
    return get_random_u32();
}

void platform_yield(void) {
    __asm__ volatile("hlt");
}
