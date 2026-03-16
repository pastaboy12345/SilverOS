#include "stdio.h"
#include "../platform/platform.h"
#include "../../include/string.h"

int printf(const char *fmt, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    int res = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    
    platform_console_write(buf);
    return res;
}

int sprintf(char *buf, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int res = vsnprintf(buf, 0xFFFFFFFF, fmt, ap); /* unsafe size but it's a wrapper */
    va_end(ap);
    return res;
}
