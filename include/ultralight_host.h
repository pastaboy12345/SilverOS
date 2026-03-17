#ifndef SILVEROS_ULTRALIGHT_HOST_H
#define SILVEROS_ULTRALIGHT_HOST_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  int x, y, w, h;        /* target region */
  const char* url;       /* eg. file:///home/user/index.html */
} ul_host_config_t;

/* Initialize/shutdown global Ultralight state. Safe to call multiple times. */
bool ul_host_init(void);
void ul_host_shutdown(void);
bool ul_host_is_available(void);
const char* ul_host_status(void);

/* Create/destroy a view instance. Returned pointer is opaque. */
void* ul_host_create_view(const ul_host_config_t* cfg);
void  ul_host_destroy_view(void* view);

/* Input */
void ul_host_mouse_move(void* view, int x, int y);
void ul_host_mouse_button(void* view, int button /*1=left*/, bool down);
void ul_host_mouse_wheel(void* view, int delta_y);
void ul_host_key_char(void* view, char ch);

/* Main loop */
void ul_host_update(void* view);
void ul_host_render(void* view);

/* Read-only frame data exposed by the active view (ARGB pixels). */
const uint32_t* ul_host_pixels(void* view, int* out_w, int* out_h);

#ifdef __cplusplus
}
#endif

#endif
