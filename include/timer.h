#ifndef SILVEROS_TIMER_H
#define SILVEROS_TIMER_H

#include "types.h"

void timer_init(uint32_t frequency);
void sleep_ms(uint32_t ms);
uint64_t timer_get_ticks(void);
uint64_t timer_get_ms(void);

#endif
