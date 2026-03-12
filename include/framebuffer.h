#ifndef SILVEROS_FRAMEBUFFER_H
#define SILVEROS_FRAMEBUFFER_H

#include "types.h"
#include "multiboot2.h"

/* Color helpers — 32-bit ARGB */
#define RGB(r, g, b)       ((uint32_t)(0xFF000000 | ((r) << 16) | ((g) << 8) | (b)))
#define RGBA(r, g, b, a)   ((uint32_t)(((a) << 24) | ((r) << 16) | ((g) << 8) | (b)))

/* Common colors */
#define COLOR_BLACK        RGB(0, 0, 0)
#define COLOR_WHITE        RGB(255, 255, 255)
#define COLOR_SILVER       RGB(192, 192, 192)
#define COLOR_DARK_SILVER  RGB(128, 128, 128)
#define COLOR_LIGHT_GRAY   RGB(200, 200, 200)
#define COLOR_DARK_GRAY    RGB(50, 50, 50)
#define COLOR_RED          RGB(220, 50, 50)
#define COLOR_GREEN        RGB(50, 200, 50)
#define COLOR_BLUE         RGB(50, 100, 220)

typedef struct {
    uint32_t *buffer;       /* Front buffer (VRAM) */
    uint32_t *back_buffer;  /* Back buffer for double buffering */
    uint32_t  width;
    uint32_t  height;
    uint32_t  pitch;        /* Bytes per scanline */
    uint8_t   bpp;          /* Bits per pixel */
} framebuffer_t;

extern framebuffer_t fb;

void     fb_init(struct multiboot2_tag_framebuffer *fb_tag);
void     fb_put_pixel(int x, int y, uint32_t color);
uint32_t fb_get_pixel(int x, int y);
void     fb_fill_rect(int x, int y, int w, int h, uint32_t color);
void     fb_draw_rect(int x, int y, int w, int h, uint32_t color);
void     fb_draw_line(int x0, int y0, int x1, int y1, uint32_t color);
void     fb_fill_screen(uint32_t color);
void     fb_draw_gradient_v(int x, int y, int w, int h, uint32_t color1, uint32_t color2);
void     fb_draw_gradient_h(int x, int y, int w, int h, uint32_t color1, uint32_t color2);
void     fb_swap_buffers(void);
void     fb_blit_region(int x, int y, int w, int h, uint32_t *src);
uint32_t fb_blend(uint32_t bg, uint32_t fg, uint8_t alpha);

#endif
