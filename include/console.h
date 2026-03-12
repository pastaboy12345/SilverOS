#ifndef SILVEROS_CONSOLE_H
#define SILVEROS_CONSOLE_H

#include "types.h"

void console_init(void);
void console_putc(char c);
void console_puts(const char *str);
void console_clear(void);
void kprintf(const char *fmt, ...);

#endif
