#include "../include/timer.h"
#include "../include/idt.h"
#include "../include/pic.h"
#include "../include/io.h"
#include "../include/serial.h"

#define PIT_CHANNEL0 0x40
#define PIT_COMMAND  0x43
#define PIT_FREQUENCY 1193182

static volatile uint64_t timer_ticks = 0;
static uint32_t timer_freq = 0;

static void timer_handler(struct interrupt_frame *frame) {
    (void)frame;
    timer_ticks++;
    pic_send_eoi(0);
}

void timer_init(uint32_t frequency) {
    timer_freq = frequency;
    uint32_t divisor = PIT_FREQUENCY / frequency;

    outb(PIT_COMMAND, 0x36);  /* Channel 0, lobyte/hibyte, rate generator */
    outb(PIT_CHANNEL0, divisor & 0xFF);
    outb(PIT_CHANNEL0, (divisor >> 8) & 0xFF);

    idt_set_handler(32, timer_handler);
    pic_clear_mask(0);  /* Unmask IRQ0 (timer) */

    serial_printf("[TIMER] PIT initialized at %d Hz\n", (uint64_t)frequency);
}

void sleep_ms(uint32_t ms) {
    uint64_t target = timer_ticks + (ms * timer_freq / 1000);
    while (timer_ticks < target) {
        __asm__ volatile ("hlt");
    }
}

uint64_t timer_get_ticks(void) {
    return timer_ticks;
}

uint64_t timer_get_ms(void) {
    if (timer_freq == 0) return 0;
    return (timer_ticks * 1000) / timer_freq;
}
