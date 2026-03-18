#include "../../include/ultralight_host.h"

#include "../../include/heap.h"
#include "../../include/serial.h"
#include "../../include/string.h"

typedef struct {
  int x;
  int y;
  int w;
  int h;
  char url[256];
} ul_view_t;

static bool g_inited = false;
static const char* k_ul_status =
    "WebKit renderer is not built into SilverOS yet.\n"
    "Web pages will target WebKit + JavaScriptCore once the port is ready.\n"
    "V8 remains a separate native scripting runtime for OS-side logic.";

extern "C" bool ul_host_init(void) {
  if (g_inited) {
    return true;
  }

  serial_printf("[WEB] %s\n", ul_host_status());
  g_inited = true;
  return true;
}

extern "C" void ul_host_shutdown(void) {
  if (!g_inited) {
    return;
  }

  g_inited = false;
}

extern "C" bool ul_host_is_available(void) {
  return false;
}

extern "C" const char* ul_host_status(void) {
  return k_ul_status;
}

extern "C" void* ul_host_create_view(const ul_host_config_t* cfg) {
  ul_view_t* view;

  if (!cfg) {
    return NULL;
  }

  view = (ul_view_t*)kmalloc(sizeof(ul_view_t));
  if (!view) {
    serial_printf("[WEB] Failed to allocate view state\n");
    return NULL;
  }

  memset(view, 0, sizeof(*view));
  view->x = cfg->x;
  view->y = cfg->y;
  view->w = cfg->w;
  view->h = cfg->h;

  if (cfg->url) {
    strncpy(view->url, cfg->url, sizeof(view->url) - 1);
  }

  serial_printf("[WEB] View initialized for %s (%dx%d)\n",
                view->url[0] ? view->url : "(no url)",
                (uint64_t)view->w,
                (uint64_t)view->h);
  return view;
}

extern "C" void ul_host_destroy_view(void* view) {
  if (!view) {
    return;
  }

  kfree(view);
}

extern "C" void ul_host_mouse_move(void* view, int x, int y) {
  (void)view;
  (void)x;
  (void)y;
}

extern "C" void ul_host_mouse_button(void* view, int button, bool down) {
  (void)view;
  (void)button;
  (void)down;
}

extern "C" void ul_host_mouse_wheel(void* view, int delta_y) {
  (void)view;
  (void)delta_y;
}

extern "C" void ul_host_key_char(void* view, char ch) {
  (void)view;
  (void)ch;
}

extern "C" void ul_host_update(void* view) {
  (void)view;
}

extern "C" void ul_host_render(void* view) {
  (void)view;
}

extern "C" const uint32_t* ul_host_pixels(void* view, int* out_w, int* out_h) {
  (void)view;

  if (out_w) {
    *out_w = 0;
  }
  if (out_h) {
    *out_h = 0;
  }
  return nullptr;
}
