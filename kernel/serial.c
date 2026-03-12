#include "../include/serial.h"
#include "../include/io.h"
#include "../include/string.h"

void serial_init(void) {
    outb(COM1 + 1, 0x00);  /* Disable interrupts */
    outb(COM1 + 3, 0x80);  /* Enable DLAB (set baud rate divisor) */
    outb(COM1 + 0, 0x03);  /* Divisor 3 = 38400 baud */
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);  /* 8 bits, no parity, one stop bit */
    outb(COM1 + 2, 0xC7);  /* Enable FIFO, clear, 14-byte threshold */
    outb(COM1 + 4, 0x0B);  /* IRQs enabled, RTS/DSR set */
}

static int serial_transmit_empty(void) {
    return inb(COM1 + 5) & 0x20;
}

void serial_putc(char c) {
    int timeout = 10000;
    while (!serial_transmit_empty() && timeout > 0) timeout--;
    if (timeout > 0) outb(COM1, c);
}

void serial_puts(const char *str) {
    while (*str) {
        if (*str == '\n') serial_putc('\r');
        serial_putc(*str++);
    }
}

void serial_printf(const char *fmt, ...) {
    char buf[512];
    typedef __builtin_va_list va_list;
    va_list ap;
    __builtin_va_start(ap, fmt);

    /* We reuse snprintf logic inline here for simplicity */
    size_t pos = 0;
    const size_t size = sizeof(buf);

    while (*fmt) {
        if (*fmt != '%') {
            if (pos < size - 1) buf[pos] = *fmt;
            pos++;
            fmt++;
            continue;
        }
        fmt++;

        bool pad_zero = false;
        int width = 0;
        if (*fmt == '0') { pad_zero = true; fmt++; }
        while (*fmt >= '0' && *fmt <= '9') {
            width = width * 10 + (*fmt - '0');
            fmt++;
        }

        char tmp[65];
        switch (*fmt) {
            case 'd': case 'i': {
                int64_t val = __builtin_va_arg(ap, int64_t);
                itoa(val, tmp, 10);
                int len = strlen(tmp);
                for (int i = len; i < width; i++) {
                    if (pos < size - 1) buf[pos] = pad_zero ? '0' : ' ';
                    pos++;
                }
                for (int i = 0; tmp[i]; i++) {
                    if (pos < size - 1) buf[pos] = tmp[i];
                    pos++;
                }
                break;
            }
            case 'u': {
                uint64_t val = __builtin_va_arg(ap, uint64_t);
                utoa(val, tmp, 10);
                int len = strlen(tmp);
                for (int i = len; i < width; i++) {
                    if (pos < size - 1) buf[pos] = pad_zero ? '0' : ' ';
                    pos++;
                }
                for (int i = 0; tmp[i]; i++) {
                    if (pos < size - 1) buf[pos] = tmp[i];
                    pos++;
                }
                break;
            }
            case 'x': {
                uint64_t val = __builtin_va_arg(ap, uint64_t);
                utoa(val, tmp, 16);
                int len = strlen(tmp);
                for (int i = len; i < width; i++) {
                    if (pos < size - 1) buf[pos] = pad_zero ? '0' : ' ';
                    pos++;
                }
                for (int i = 0; tmp[i]; i++) {
                    if (pos < size - 1) buf[pos] = tmp[i];
                    pos++;
                }
                break;
            }
            case 'p': {
                uint64_t val = __builtin_va_arg(ap, uint64_t);
                utoa(val, tmp, 16);
                if (pos < size - 1) buf[pos] = '0'; pos++;
                if (pos < size - 1) buf[pos] = 'x'; pos++;
                for (int i = 0; tmp[i]; i++) {
                    if (pos < size - 1) buf[pos] = tmp[i];
                    pos++;
                }
                break;
            }
            case 's': {
                const char *s = __builtin_va_arg(ap, const char *);
                if (!s) s = "(null)";
                while (*s) {
                    if (pos < size - 1) buf[pos] = *s;
                    pos++; s++;
                }
                break;
            }
            case 'c': {
                char c = (char)__builtin_va_arg(ap, int);
                if (pos < size - 1) buf[pos] = c;
                pos++;
                break;
            }
            case '%':
                if (pos < size - 1) buf[pos] = '%';
                pos++;
                break;
            default:
                if (pos < size - 1) buf[pos] = '%'; pos++;
                if (pos < size - 1) buf[pos] = *fmt; pos++;
                break;
        }
        fmt++;
    }

    if (size > 0) buf[pos < size ? pos : size - 1] = 0;
    __builtin_va_end(ap);

    serial_puts(buf);
}
