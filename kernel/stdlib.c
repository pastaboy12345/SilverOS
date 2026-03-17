#include "../include/stdlib.h"
#include "../include/heap.h"
#include "../include/serial.h"
#include "../include/io.h"

void *malloc(size_t size) {
    return kmalloc(size);
}

void *calloc(size_t count, size_t size) {
    return kcalloc(count, size);
}

void *realloc(void *ptr, size_t new_size) {
    return krealloc(ptr, new_size);
}

void free(void *ptr) {
    kfree(ptr);
}

void abort(void) {
    serial_printf("[KERNEL] ABORT called\n");
    cli();
    while (1) hlt();
}

void exit(int status) {
    serial_printf("[KERNEL] EXIT called with status %d\n", status);
    cli();
    while (1) hlt();
}

double strtod(const char *nptr, char **endptr) {
    double res = 0.0;
    double factor = 1.0;
    bool decimal = false;
    const char *p = nptr;

    while (*p == ' ') p++;
    if (*p == '-') { factor = -1.0; p++; }
    else if (*p == '+') p++;

    for (; *p; p++) {
        if (*p >= '0' && *p <= '9') {
            if (decimal) {
                res += (*p - '0') * (factor / 10.0);
                factor /= 10.0;
            } else {
                res = res * 10.0 + (*p - '0');
            }
        } else if (*p == '.' && !decimal) {
            decimal = true;
            if (factor > 0) factor = 1.0;
            else factor = -1.0;
        } else {
            break;
        }
    }

    if (endptr) *endptr = (char *)p;
    return res * (factor < 0 && !decimal ? -1.0 : 1.0);
}

