#include "../include/desktop.h"
#include "../include/font.h"
#include "../include/framebuffer.h"
#include "../include/mouse.h"
#include "../include/string.h"
#include "../include/ultralight_host.h"

static int ul_window_id = -1;
static void* ul_view = NULL;
static char ul_url[256] = "file:///home/user/index.html";

static void ul_draw_multiline(int x, int y, const char* text, uint32_t color) {
  char line[96];
  int row = 0;

  while (text && *text) {
    int len = 0;
    while (text[len] && text[len] != '\n' && len < (int)sizeof(line) - 1) {
      line[len] = text[len];
      len++;
    }

    line[len] = 0;
    font_draw_string(x, y + row * (FONT_HEIGHT + 2), line, color);
    row++;

    text += len;
    if (*text == '\n') {
      text++;
    }
  }
}

static void ul_blit_scaled_argb(int dx, int dy, int dw, int dh, const uint32_t* src, int sw, int sh) {
  int x;
  int y;

  if (!src || sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0) {
    return;
  }

  for (y = 0; y < dh; ++y) {
    int sy = (y * sh) / dh;
    const uint32_t* src_row = src + (size_t)sy * (size_t)sw;
    for (x = 0; x < dw; ++x) {
      int sx = (x * sw) / dw;
      fb_put_pixel(dx + x, dy + y, src_row[sx]);
    }
  }
}

static void ul_draw_status_overlay(int x, int y, int w) {
  int panel_h = 76;

  fb_fill_rect(x + 8, y + 8, w - 16, panel_h, RGB(9, 14, 23));
  fb_draw_rect(x + 8, y + 8, w - 16, panel_h, RGB(61, 85, 116));
  font_draw_string(x + 18, y + 18, "Ultralight SDK Compatibility Mode", RGB(233, 240, 250));
  font_draw_string(x + 18, y + 36, ul_url, RGB(143, 174, 217));
  if (!ul_host_is_available()) {
    font_draw_string(x + 18, y + 56, "SDK frame is unavailable.", RGB(255, 189, 116));
  }
}

static void ul_draw(int id) {
  mouse_state_t ms;
  const uint32_t* pixels;
  int sw = 0;
  int sh = 0;
  window_t* win = window_get(id);
  if (!win) {
    return;
  }

  int cx = win->x + WINDOW_BORDER;
  int cy = win->y + TITLEBAR_HEIGHT + 2;
  int cw = win->width - WINDOW_BORDER * 2;
  int ch = win->height - TITLEBAR_HEIGHT - WINDOW_BORDER - 2;

  fb_fill_rect(cx, cy, cw, ch, RGB(10, 14, 24));

  if (!ul_view) {
    ul_draw_multiline(cx + 18, cy + 22, ul_host_status(), RGB(194, 204, 220));
    return;
  }

  mouse_get_state(&ms);
  if (ms.x >= cx && ms.x < cx + cw && ms.y >= cy && ms.y < cy + ch) {
    ul_host_mouse_move(ul_view, ms.x - cx, ms.y - cy);
  }

  ul_host_update(ul_view);
  ul_host_render(ul_view);

  pixels = ul_host_pixels(ul_view, &sw, &sh);
  if (pixels && sw > 0 && sh > 0) {
    ul_blit_scaled_argb(cx, cy, cw, ch, pixels, sw, sh);
  } else {
    ul_draw_multiline(cx + 18, cy + 22, ul_host_status(), RGB(194, 204, 220));
  }

  ul_draw_status_overlay(cx, cy, cw);
}

static void ul_key_handler(int id, char key) {
  (void)id;
  if (!ul_view) {
    return;
  }
  if (key >= 0x20 && key < 0x7F) {
    ul_host_key_char(ul_view, key);
  }
}

static void ul_click_handler(int id, int mx, int my) {
  window_t* win = window_get(id);
  if (!win || !ul_view) {
    return;
  }

  int cx = win->x + WINDOW_BORDER;
  int cy = win->y + TITLEBAR_HEIGHT + 2;
  int local_x = mx - cx;
  int local_y = my - cy;

  if (local_x < 0 || local_y < 0 || local_x >= win->content_w || local_y >= win->content_h) {
    return;
  }

  ul_host_mouse_move(ul_view, local_x, local_y);
  ul_host_mouse_button(ul_view, 1, true);
  ul_host_mouse_button(ul_view, 1, false);
}

void ultralight_open(const char* url) {
  if (ul_window_id >= 0 && window_get(ul_window_id)) {
    return;
  }

  if (ul_view) {
    ul_host_destroy_view(ul_view);
    ul_view = NULL;
  }

  if (url && url[0]) {
    strncpy(ul_url, url, sizeof(ul_url) - 1);
    ul_url[sizeof(ul_url) - 1] = 0;
  }

  ul_host_init();

  ul_window_id = window_create("Ultralight", 140, 90, 700, 500);
  if (ul_window_id < 0) {
    return;
  }

  ul_host_config_t cfg;
  cfg.x = 0;
  cfg.y = 0;
  cfg.w = 700 - WINDOW_BORDER * 2;
  cfg.h = 500 - TITLEBAR_HEIGHT - WINDOW_BORDER - 2;
  cfg.url = ul_url;

  ul_view = ul_host_create_view(&cfg);

  window_set_draw_callback(ul_window_id, ul_draw);
  window_set_key_callback(ul_window_id, ul_key_handler);
  window_set_click_callback(ul_window_id, ul_click_handler);
}
