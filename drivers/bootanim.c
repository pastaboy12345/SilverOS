#include "../include/bootanim.h"
#include "../include/framebuffer.h"
#include "../include/font.h"
#include "../include/timer.h"
#include "../include/string.h"
#include "../include/serial.h"

/* Silver theme colors */
#define BG_TOP       RGB(12, 12, 30)     /* Deep navy/black */
#define BG_BOTTOM    RGB(22, 33, 62)     /* Dark blue-gray */
#define SILVER_BRIGHT RGB(220, 225, 230) /* Bright silver */
#define SILVER_MID    RGB(170, 175, 180) /* Medium silver */
#define SILVER_DARK   RGB(100, 105, 110) /* Dark silver */
#define SILVER_GLOW   RGB(200, 210, 230) /* Silver-blue glow */
#define BAR_BG        RGB(40, 42, 54)    /* Progress bar background */
#define BAR_FILL      RGB(180, 190, 200) /* Progress bar fill */
#define BAR_GLOW      RGB(140, 160, 200) /* Glow on bar */

static const char *status_messages[] = {
    "Initializing hardware...",
    "Loading memory manager...",
    "Starting SilverFS...",
    "Preparing desktop...",
    "Almost ready...",
    "Welcome to SilverOS"
};

static void draw_shimmer_text(const char *text, int cx, int cy, int scale, int shimmer_x) {
    int text_width = strlen(text) * FONT_WIDTH * scale;
    int start_x = cx - text_width / 2;

    for (int i = 0; text[i]; i++) {
        int char_x = start_x + i * FONT_WIDTH * scale;
        int dist = char_x - shimmer_x;
        if (dist < 0) dist = -dist;

        uint32_t color;
        if (dist < 30) {
            color = RGB(255, 255, 255);  /* Bright white shimmer */
        } else if (dist < 80) {
            uint8_t blend = 255 - (dist - 30) * 3;
            color = RGB(blend, blend, (blend + 20 > 255) ? 255 : blend + 20);
        } else {
            color = SILVER_MID;
        }

        font_draw_string_scaled(char_x, cy, (char[]){text[i], 0}, color, scale);
    }
}

static void draw_progress_bar(int x, int y, int w, int h, int progress, int max_progress) {
    /* Background */
    fb_fill_rect(x, y, w, h, BAR_BG);

    /* Border with silver */
    fb_draw_rect(x, y, w, h, SILVER_DARK);

    /* Fill */
    int fill_w = (w - 4) * progress / max_progress;
    if (fill_w > 0) {
        /* Gradient fill — darker silver to bright silver */
        for (int i = 0; i < fill_w; i++) {
            uint8_t bright = 140 + (i * 80 / (w - 4));
            uint32_t col = RGB(bright, bright + 5, bright + 15);
            for (int j = 2; j < h - 2; j++) {
                /* Slight vertical gradient for 3D effect */
                int vbright = bright + (j < h / 2 ? 15 : -10);
                if (vbright > 255) vbright = 255;
                if (vbright < 0) vbright = 0;
                fb_put_pixel(x + 2 + i, y + j, RGB(vbright, vbright + 3, vbright + 10));
            }
        }

        /* Glow on leading edge */
        int edge_x = x + 2 + fill_w;
        for (int j = 1; j < h - 1; j++) {
            fb_put_pixel(edge_x, y + j, SILVER_GLOW);
            if (edge_x + 1 < x + w - 2)
                fb_put_pixel(edge_x + 1, y + j, fb_blend(BAR_BG, SILVER_GLOW, 100));
        }
    }
}

static void draw_particles(int frame) {
    /* Floating silver particles */
    for (int i = 0; i < 20; i++) {
        int px = (i * 137 + frame * 3) % (int)fb.width;
        int py = (i * 97 + frame) % (int)fb.height;
        uint8_t alpha = 50 + (i * 23 + frame * 7) % 100;
        uint32_t color = RGB(alpha + 80, alpha + 85, alpha + 100);
        fb_put_pixel(px, py, color);
        fb_put_pixel(px + 1, py, color);
    }
}

void bootanim_play(void) {
    serial_printf("[BOOTANIM] Starting boot animation...\n");

    int center_x = fb.width / 2;
    int center_y = fb.height / 2 - 60;

    int bar_w = 400;
    int bar_h = 18;
    int bar_x = center_x - bar_w / 2;
    int bar_y = center_y + 100;

    int total_frames = 180;  /* ~3 seconds at 60fps, but our timer is slower */
    int status_idx = 0;

    for (int frame = 0; frame < total_frames; frame++) {
        /* Draw dark gradient background */
        fb_draw_gradient_v(0, 0, fb.width, fb.height, BG_TOP, BG_BOTTOM);

        /* Draw particles */
        draw_particles(frame);

        /* Draw subtle radial glow behind logo */
        int glow_r = 150 + (frame % 30);
        for (int dy = -glow_r; dy < glow_r; dy += 3) {
            for (int dx = -glow_r; dx < glow_r; dx += 3) {
                int dist = dx * dx + dy * dy;
                if (dist < glow_r * glow_r) {
                    int alpha = 25 - (dist * 25 / (glow_r * glow_r));
                    if (alpha > 0) {
                        int px = center_x + dx;
                        int py = center_y + dy;
                        uint32_t bg = fb_get_pixel(px, py);
                        fb_put_pixel(px, py, fb_blend(bg, SILVER_GLOW, alpha));
                    }
                }
            }
        }

        /* Draw "SilverOS" with shimmer effect */
        int shimmer_pos = ((frame * 12) % (int)(fb.width + 200)) - 100;
        draw_shimmer_text("SilverOS", center_x, center_y, 4, shimmer_pos);

        /* Draw subtitle */
        const char *subtitle = "x86_64 Operating System";
        int sub_w = strlen(subtitle) * FONT_WIDTH;
        font_draw_string(center_x - sub_w / 2, center_y + 70, subtitle, SILVER_DARK);

        /* Progress bar */
        int progress = frame * 100 / total_frames;
        draw_progress_bar(bar_x, bar_y, bar_w, bar_h, progress, 100);

        /* Status message */
        status_idx = frame * 6 / total_frames;
        if (status_idx >= 6) status_idx = 5;
        const char *msg = status_messages[status_idx];
        int msg_w = strlen(msg) * FONT_WIDTH;
        font_draw_string(center_x - msg_w / 2, bar_y + 30, msg, SILVER_DARK);

        /* Draw version number */
        font_draw_string(fb.width - 100, fb.height - 20, "v0.1.0", SILVER_DARK);

        /* Swap and wait */
        fb_swap_buffers();
        sleep_ms(16);  /* ~60 fps target */
    }

    /* Fade out to black */
    for (int alpha = 0; alpha < 255; alpha += 5) {
        fb_draw_gradient_v(0, 0, fb.width, fb.height, BG_TOP, BG_BOTTOM);
        draw_shimmer_text("SilverOS", center_x, center_y, 4, center_x);

        /* Overlay black with increasing alpha */
        for (uint32_t y = 0; y < fb.height; y++) {
            for (uint32_t x = 0; x < fb.width; x++) {
                uint32_t pixel = fb_get_pixel(x, y);
                fb_put_pixel(x, y, fb_blend(pixel, COLOR_BLACK, alpha));
            }
        }

        fb_swap_buffers();
        sleep_ms(10);
    }

    fb_fill_screen(COLOR_BLACK);
    fb_swap_buffers();

    serial_printf("[BOOTANIM] Boot animation complete\n");
}
