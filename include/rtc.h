#ifndef SILVEROS_RTC_H
#define SILVEROS_RTC_H

#include "types.h"

typedef struct {
    uint16_t year;
    uint8_t  month;
    uint8_t  day;
    uint8_t  hour;
    uint8_t  minute;
    uint8_t  second;
} rtc_time_t;

void rtc_init(void);
void rtc_get_time(rtc_time_t *time);

#endif
