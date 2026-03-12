#include "../include/framebuffer.h"
#include "../include/string.h"
#include "../include/serial.h"

framebuffer_t fb;

/* Static back buffer — for 1920x1080x4 = ~8 MB */
static uint32_t back_buf[1920 * 1080];

void fb_init(struct multiboot2_tag_framebuffer *fb_tag) {
    fb.buffer      = (uint32_t *)(uintptr_t)fb_tag->framebuffer_addr;
    fb.width       = fb_tag->framebuffer_width;
    fb.height      = fb_tag->framebuffer_height;
    fb.pitch       = fb_tag->framebuffer_pitch;
    fb.bpp         = fb_tag->framebuffer_bpp;
    fb.back_buffer = back_buf;

    memset(fb.back_buffer, 0, fb.width * fb.height * 4);

    serial_printf("[FB] Initialized: %dx%d, %d bpp, addr=0x%x\n",
                  (uint64_t)fb.width, (uint64_t)fb.height,
                  (uint64_t)fb.bpp, fb_tag->framebuffer_addr);
}

void fb_put_pixel(int x, int y, uint32_t color) {
    if (x < 0 || y < 0 || x >= (int)fb.width || y >= (int)fb.height) return;
    fb.back_buffer[y * fb.width + x] = color;
}

uint32_t fb_get_pixel(int x, int y) {
    if (x < 0 || y < 0 || x >= (int)fb.width || y >= (int)fb.height) return 0;
    return fb.back_buffer[y * fb.width + x];
}

void fb_fill_rect(int x, int y, int w, int h, uint32_t color) {
    for (int j = y; j < y + h; j++) {
        if (j < 0 || j >= (int)fb.height) continue;
        for (int i = x; i < x + w; i++) {
            if (i < 0 || i >= (int)fb.width) continue;
            fb.back_buffer[j * fb.width + i] = color;
        }
    }
}

void fb_draw_rect(int x, int y, int w, int h, uint32_t color) {
    for (int i = x; i < x + w; i++) {
        fb_put_pixel(i, y, color);
        fb_put_pixel(i, y + h - 1, color);
    }
    for (int j = y; j < y + h; j++) {
        fb_put_pixel(x, j, color);
        fb_put_pixel(x + w - 1, j, color);
    }
}

void fb_draw_line(int x0, int y0, int x1, int y1, uint32_t color) {
    int dx = x1 - x0;
    int dy = y1 - y0;
    int sx = dx > 0 ? 1 : -1;
    int sy = dy > 0 ? 1 : -1;
    dx = dx > 0 ? dx : -dx;
    dy = dy > 0 ? dy : -dy;

    int err = (dx > dy ? dx : -dy) / 2;
    int e2;

    while (1) {
        fb_put_pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        e2 = err;
        if (e2 > -dx) { err -= dy; x0 += sx; }
        if (e2 <  dy) { err += dx; y0 += sy; }
    }
}

void fb_fill_screen(uint32_t color) {
    for (uint32_t i = 0; i < fb.width * fb.height; i++) {
        fb.back_buffer[i] = color;
    }
}

void fb_draw_gradient_v(int x, int y, int w, int h, uint32_t color1, uint32_t color2) {
    uint8_t r1 = (color1 >> 16) & 0xFF, g1 = (color1 >> 8) & 0xFF, b1 = color1 & 0xFF;
    uint8_t r2 = (color2 >> 16) & 0xFF, g2 = (color2 >> 8) & 0xFF, b2 = color2 & 0xFF;

    for (int j = 0; j < h; j++) {
        uint8_t r = r1 + (r2 - r1) * j / h;
        uint8_t g = g1 + (g2 - g1) * j / h;
        uint8_t b = b1 + (b2 - b1) * j / h;
        uint32_t color = RGB(r, g, b);
        for (int i = 0; i < w; i++) {
            fb_put_pixel(x + i, y + j, color);
        }
    }
}

void fb_draw_gradient_h(int x, int y, int w, int h, uint32_t color1, uint32_t color2) {
    uint8_t r1 = (color1 >> 16) & 0xFF, g1 = (color1 >> 8) & 0xFF, b1 = color1 & 0xFF;
    uint8_t r2 = (color2 >> 16) & 0xFF, g2 = (color2 >> 8) & 0xFF, b2 = color2 & 0xFF;

    for (int i = 0; i < w; i++) {
        uint8_t r = r1 + (r2 - r1) * i / w;
        uint8_t g = g1 + (g2 - g1) * i / w;
        uint8_t b = b1 + (b2 - b1) * i / w;
        uint32_t color = RGB(r, g, b);
        for (int j = 0; j < h; j++) {
            fb_put_pixel(x + i, y + j, color);
        }
    }
}

void fb_swap_buffers(void) {
    /* Copy back buffer to front buffer (VRAM) */
    uint32_t pitch_pixels = fb.pitch / 4;
    for (uint32_t y = 0; y < fb.height; y++) {
        memcpy(&fb.buffer[y * pitch_pixels],
               &fb.back_buffer[y * fb.width],
               fb.width * 4);
    }
}

void fb_blit_region(int x, int y, int w, int h, uint32_t *src) {
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            fb_put_pixel(x + i, y + j, src[j * w + i]);
        }
    }
}

uint32_t fb_blend(uint32_t bg, uint32_t fg, uint8_t alpha) {
    uint8_t br = (bg >> 16) & 0xFF, bg_g = (bg >> 8) & 0xFF, bb = bg & 0xFF;
    uint8_t fr = (fg >> 16) & 0xFF, fg_g = (fg >> 8) & 0xFF, fb_b = fg & 0xFF;

    uint8_t r = (fr * alpha + br * (255 - alpha)) / 255;
    uint8_t g = (fg_g * alpha + bg_g * (255 - alpha)) / 255;
    uint8_t b = (fb_b * alpha + bb * (255 - alpha)) / 255;

    return RGB(r, g, b);
}
