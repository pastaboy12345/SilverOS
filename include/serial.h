#ifndef SILVEROS_SERIAL_H
#define SILVEROS_SERIAL_H

#include "types.h"

#define COM1 0x3F8

#ifdef __cplusplus
extern "C" {
#endif

void serial_init(void);
void serial_putc(char c);
void serial_puts(const char *str);
void serial_printf(const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif
