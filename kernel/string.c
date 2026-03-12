#include "../include/string.h"
#include "../include/types.h"

/* ---- Memory functions ---- */

void *memset(void *dest, int val, size_t count) {
    uint8_t *d = (uint8_t *)dest;
    for (size_t i = 0; i < count; i++)
        d[i] = (uint8_t)val;
    return dest;
}

void *memcpy(void *dest, const void *src, size_t count) {
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;
    for (size_t i = 0; i < count; i++)
        d[i] = s[i];
    return dest;
}

void *memmove(void *dest, const void *src, size_t count) {
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;
    if (d < s) {
        for (size_t i = 0; i < count; i++)
            d[i] = s[i];
    } else {
        for (size_t i = count; i > 0; i--)
            d[i - 1] = s[i - 1];
    }
    return dest;
}

int memcmp(const void *s1, const void *s2, size_t count) {
    const uint8_t *a = (const uint8_t *)s1;
    const uint8_t *b = (const uint8_t *)s2;
    for (size_t i = 0; i < count; i++) {
        if (a[i] != b[i])
            return a[i] - b[i];
    }
    return 0;
}

/* ---- String functions ---- */

size_t strlen(const char *str) {
    size_t len = 0;
    while (str[len]) len++;
    return len;
}

int strcmp(const char *s1, const char *s2) {
    while (*s1 && *s1 == *s2) { s1++; s2++; }
    return *(uint8_t *)s1 - *(uint8_t *)s2;
}

int strncmp(const char *s1, const char *s2, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (s1[i] != s2[i]) return (uint8_t)s1[i] - (uint8_t)s2[i];
        if (s1[i] == 0) return 0;
    }
    return 0;
}

char *strcpy(char *dest, const char *src) {
    char *d = dest;
    while ((*d++ = *src++));
    return dest;
}

char *strncpy(char *dest, const char *src, size_t n) {
    size_t i;
    for (i = 0; i < n && src[i]; i++)
        dest[i] = src[i];
    for (; i < n; i++)
        dest[i] = 0;
    return dest;
}

char *strcat(char *dest, const char *src) {
    char *d = dest + strlen(dest);
    while ((*d++ = *src++));
    return dest;
}

char *strchr(const char *s, int c) {
    while (*s) {
        if (*s == (char)c) return (char *)s;
        s++;
    }
    if (c == 0) return (char *)s;
    return NULL;
}

/* ---- Number to string conversion ---- */

void itoa(int64_t value, char *buf, int base) {
    if (base < 2 || base > 16) { buf[0] = 0; return; }

    char tmp[65];
    int i = 0;
    bool negative = false;

    if (value < 0 && base == 10) {
        negative = true;
        value = -value;
    }

    uint64_t uval = (uint64_t)value;
    do {
        int digit = uval % base;
        tmp[i++] = digit < 10 ? '0' + digit : 'a' + digit - 10;
        uval /= base;
    } while (uval);

    if (negative) tmp[i++] = '-';

    int j = 0;
    while (i > 0) buf[j++] = tmp[--i];
    buf[j] = 0;
}

void utoa(uint64_t value, char *buf, int base) {
    if (base < 2 || base > 16) { buf[0] = 0; return; }

    char tmp[65];
    int i = 0;

    do {
        int digit = value % base;
        tmp[i++] = digit < 10 ? '0' + digit : 'a' + digit - 10;
        value /= base;
    } while (value);

    int j = 0;
    while (i > 0) buf[j++] = tmp[--i];
    buf[j] = 0;
}

int atoi(const char *str) {
    int result = 0;
    int sign = 1;
    while (*str == ' ') str++;
    if (*str == '-') { sign = -1; str++; }
    else if (*str == '+') { str++; }
    while (*str >= '0' && *str <= '9') {
        result = result * 10 + (*str - '0');
        str++;
    }
    return result * sign;
}

/* ---- Simple snprintf ---- */
/* Supports: %d, %u, %x, %s, %c, %p, %% */

/* We use GCC builtins for va_list in freestanding mode */
typedef __builtin_va_list va_list;
#define va_start(ap, last) __builtin_va_start(ap, last)
#define va_arg(ap, type)   __builtin_va_arg(ap, type)
#define va_end(ap)         __builtin_va_end(ap)

static void snprintf_putc(char *buf, size_t size, size_t *pos, char c) {
    if (*pos < size - 1) buf[*pos] = c;
    (*pos)++;
}

static void snprintf_puts(char *buf, size_t size, size_t *pos, const char *s) {
    while (*s) snprintf_putc(buf, size, pos, *s++);
}

int vsnprintf(char *buf, size_t size, const char *fmt, va_list ap) {
    size_t pos = 0;
    char tmp[65];

    while (*fmt) {
        if (*fmt != '%') {
            snprintf_putc(buf, size, &pos, *fmt++);
            continue;
        }
        fmt++; /* skip '%' */

        /* Handle padding */
        bool pad_zero = false;
        int width = 0;
        if (*fmt == '0') { pad_zero = true; fmt++; }
        while (*fmt >= '0' && *fmt <= '9') {
            width = width * 10 + (*fmt - '0');
            fmt++;
        }

        switch (*fmt) {
            case 'd': case 'i': {
                int64_t val = va_arg(ap, int64_t);
                itoa(val, tmp, 10);
                int len = strlen(tmp);
                for (int i = len; i < width; i++)
                    snprintf_putc(buf, size, &pos, pad_zero ? '0' : ' ');
                snprintf_puts(buf, size, &pos, tmp);
                break;
            }
            case 'u': {
                uint64_t val = va_arg(ap, uint64_t);
                utoa(val, tmp, 10);
                int len = strlen(tmp);
                for (int i = len; i < width; i++)
                    snprintf_putc(buf, size, &pos, pad_zero ? '0' : ' ');
                snprintf_puts(buf, size, &pos, tmp);
                break;
            }
            case 'x': {
                uint64_t val = va_arg(ap, uint64_t);
                utoa(val, tmp, 16);
                int len = strlen(tmp);
                for (int i = len; i < width; i++)
                    snprintf_putc(buf, size, &pos, pad_zero ? '0' : ' ');
                snprintf_puts(buf, size, &pos, tmp);
                break;
            }
            case 'p': {
                uint64_t val = va_arg(ap, uint64_t);
                snprintf_puts(buf, size, &pos, "0x");
                utoa(val, tmp, 16);
                snprintf_puts(buf, size, &pos, tmp);
                break;
            }
            case 's': {
                const char *s = va_arg(ap, const char *);
                if (!s) s = "(null)";
                snprintf_puts(buf, size, &pos, s);
                break;
            }
            case 'c': {
                char c = (char)va_arg(ap, int);
                snprintf_putc(buf, size, &pos, c);
                break;
            }
            case '%':
                snprintf_putc(buf, size, &pos, '%');
                break;
            default:
                snprintf_putc(buf, size, &pos, '%');
                snprintf_putc(buf, size, &pos, *fmt);
                break;
        }
        fmt++;
    }

    if (size > 0) buf[pos < size ? pos : size - 1] = 0;
    return (int)pos;
}

int snprintf(char *buf, size_t size, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int res = vsnprintf(buf, size, fmt, ap);
    va_end(ap);
    return res;
}
