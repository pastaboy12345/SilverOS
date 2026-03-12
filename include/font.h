#ifndef SILVEROS_FONT_H
#define SILVEROS_FONT_H

#include "types.h"

#define FONT_WIDTH  8
#define FONT_HEIGHT 16

void font_draw_char(int x, int y, char c, uint32_t color);
void font_draw_string(int x, int y, const char *str, uint32_t color);
void font_draw_string_scaled(int x, int y, const char *str, uint32_t color, int scale);

#endif
