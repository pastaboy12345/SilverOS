#include "../include/rtc.h"
#include "../include/io.h"

#define CMOS_ADDR 0x70
#define CMOS_DATA 0x71

static int get_update_in_progress_flag() {
    outb(CMOS_ADDR, 0x0A);
    int timeout = 100000;
    while (timeout--) {
        if (!(inb(CMOS_DATA) & 0x80)) return 0;
    }
    return 1; // Timeout (stuck)
}

static uint8_t get_RTC_register(int reg) {
    outb(CMOS_ADDR, reg);
    return inb(CMOS_DATA);
}

void rtc_init(void) {
    // Nothing strictly needed unless we're enabling interrupts
}

void rtc_get_time(rtc_time_t *t) {
    uint8_t last_second, last_minute, last_hour, last_day, last_month, last_year, last_century;
    uint8_t registerB;
    uint8_t century = 0;

    // Wait until update in progress clears
    while (get_update_in_progress_flag());

    t->second = get_RTC_register(0x00);
    t->minute = get_RTC_register(0x02);
    t->hour   = get_RTC_register(0x04);
    t->day    = get_RTC_register(0x07);
    t->month  = get_RTC_register(0x08);
    t->year   = get_RTC_register(0x09);
    century   = get_RTC_register(0x32); // Note: 0x32 might not be reliable on all PCs, but works in QEMU usually.

    do {
        last_second  = t->second;
        last_minute  = t->minute;
        last_hour    = t->hour;
        last_day     = t->day;
        last_month   = t->month;
        last_year    = t->year;
        last_century = century;

        while (get_update_in_progress_flag());

        t->second = get_RTC_register(0x00);
        t->minute = get_RTC_register(0x02);
        t->hour   = get_RTC_register(0x04);
        t->day    = get_RTC_register(0x07);
        t->month  = get_RTC_register(0x08);
        t->year   = get_RTC_register(0x09);
        century   = get_RTC_register(0x32);
    } while ( (last_second != t->second) || (last_minute != t->minute) || (last_hour != t->hour) ||
              (last_day != t->day) || (last_month != t->month) || (last_year != t->year) ||
              (last_century != century) );

    registerB = get_RTC_register(0x0B);

    // Convert BCD to binary values if necessary
    if (!(registerB & 0x04)) {
        t->second = (t->second & 0x0F) + ((t->second / 16) * 10);
        t->minute = (t->minute & 0x0F) + ((t->minute / 16) * 10);
        t->hour = ( (t->hour & 0x0F) + (((t->hour & 0x70) / 16) * 10) ) | (t->hour & 0x80);
        t->day = (t->day & 0x0F) + ((t->day / 16) * 10);
        t->month = (t->month & 0x0F) + ((t->month / 16) * 10);
        t->year = (t->year & 0x0F) + ((t->year / 16) * 10);
        if (century != 0) {
            century = (century & 0x0F) + ((century / 16) * 10);
        }
    }

    // Convert 12 hour clock to 24 hour clock if necessary
    if (!(registerB & 0x02) && (t->hour & 0x80)) {
        t->hour = ((t->hour & 0x7F) + 12) % 24;
    }

    // Calculate full year (assuming century is 20 if 0 for simplicity)
    if (century == 0) {
        century = 20; // Default to 2000s
    }
    
    t->year += century * 100;
}
