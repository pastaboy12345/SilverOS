#ifndef SILVEROS_SILVERUI_H
#define SILVEROS_SILVERUI_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t silverui_node_t;

typedef enum {
    SILVERUI_NODE_VIEW = 1,
    SILVERUI_NODE_TEXT = 2,
} silverui_node_type_t;

typedef struct {
    int x, y, w, h;
    int padding;
    uint32_t bg;
    uint32_t fg;
    bool has_bg;
    bool has_fg;
} silverui_style_t;

void silverui_init(int root_w, int root_h);
void silverui_reset(void);

silverui_node_t silverui_create_view(void);
silverui_node_t silverui_create_text(const char *text);

void silverui_set_text(silverui_node_t node, const char *text);
void silverui_set_style(silverui_node_t node, const silverui_style_t *style);

bool silverui_append_child(silverui_node_t parent, silverui_node_t child);
void silverui_clear_children(silverui_node_t parent);

void silverui_set_root(silverui_node_t node);
void silverui_layout(void);
void silverui_paint(void);

#ifdef __cplusplus
}
#endif

#endif
