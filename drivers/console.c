#include "../include/console.h"
#include "../include/framebuffer.h"
#include "../include/font.h"
#include "../include/string.h"
#include "../include/serial.h"

#define CONSOLE_BG   RGB(15, 15, 25)
#define CONSOLE_FG   RGB(200, 205, 210)

static int cursor_x = 0;
static int cursor_y = 0;

void console_init(void) {
    cursor_x = 0;
    cursor_y = 0;
}

static void console_scroll(void) {
    uint32_t rows = fb.height / FONT_HEIGHT;
    /* Move everything up one line */
    for (uint32_t y = FONT_HEIGHT; y < fb.height; y++) {
        memcpy(&fb.back_buffer[(y - FONT_HEIGHT) * fb.width],
               &fb.back_buffer[y * fb.width],
               fb.width * 4);
    }
    /* Clear bottom line */
    fb_fill_rect(0, (rows - 1) * FONT_HEIGHT, fb.width, FONT_HEIGHT, CONSOLE_BG);
}

void console_putc(char c) {
    uint32_t cols = fb.width / FONT_WIDTH;
    uint32_t rows = fb.height / FONT_HEIGHT;

    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
    } else if (c == '\r') {
        cursor_x = 0;
    } else if (c == '\t') {
        cursor_x = (cursor_x + 4) & ~3;
    } else if (c == '\b') {
        if (cursor_x > 0) {
            cursor_x--;
            fb_fill_rect(cursor_x * FONT_WIDTH, cursor_y * FONT_HEIGHT,
                         FONT_WIDTH, FONT_HEIGHT, CONSOLE_BG);
        }
    } else {
        font_draw_char(cursor_x * FONT_WIDTH, cursor_y * FONT_HEIGHT, c, CONSOLE_FG);
        cursor_x++;
    }

    if (cursor_x >= (int)cols) {
        cursor_x = 0;
        cursor_y++;
    }

    while (cursor_y >= (int)rows) {
        console_scroll();
        cursor_y--;
    }
}

void console_puts(const char *str) {
    while (*str) {
        console_putc(*str++);
    }
    fb_swap_buffers();
}

void console_clear(void) {
    fb_fill_screen(CONSOLE_BG);
    cursor_x = 0;
    cursor_y = 0;
    fb_swap_buffers();
}

int vsnprintf(char *buf, size_t size, const char *fmt, va_list ap);

void kprintf(const char *fmt, ...) {
    char buf[512];
    va_list ap;
    __builtin_va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    __builtin_va_end(ap);
    
    console_puts(buf);
    serial_puts(buf);
}
