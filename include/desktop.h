#ifndef SILVEROS_DESKTOP_H
#define SILVEROS_DESKTOP_H

#include "types.h"

/* Window structure */
#define MAX_WINDOWS 16
#define TITLEBAR_HEIGHT 24
#define WINDOW_BORDER 2
#define CLOSE_BTN_SIZE 16

typedef struct {
    int      x, y;
    int      width, height;
    char     title[64];
    bool     visible;
    bool     focused;
    bool     dragging;
    int      drag_offset_x, drag_offset_y;
    uint32_t *content;       /* Window content buffer */
    int      content_w, content_h;
    void     (*draw_content)(int window_id);   /* Custom draw callback */
    void     (*handle_key)(int window_id, char key); /* Key handler */
    void     (*handle_click)(int window_id, int mx, int my); /* Click handler */
    int      id;
} window_t;

/* Desktop API */
void    desktop_init(void);
void    desktop_run(void);

/* Window manager API */
int     window_create(const char *title, int x, int y, int w, int h);
void    window_destroy(int id);
void    window_set_draw_callback(int id, void (*draw)(int));
void    window_set_key_callback(int id, void (*handler)(int, char));
void    window_set_click_callback(int id, void (*handler)(int, int, int));
window_t *window_get(int id);

/* Terminal app */
void    terminal_open(void);

/* File Browser app */
void    file_browser_open(const char *path);

#endif
