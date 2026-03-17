#include "../../include/silverui.h"
#include "../../include/framebuffer.h"
#include "../../include/font.h"
#include "../../include/heap.h"
#include "../../include/string.h"

#define SILVERUI_MAX_NODES 1024
#define SILVERUI_MAX_CHILDREN 64

typedef struct {
    bool used;
    silverui_node_type_t type;
    silverui_style_t style;

    /* Layout results */
    int lx, ly, lw, lh;

    /* Tree */
    silverui_node_t parent;
    silverui_node_t children[SILVERUI_MAX_CHILDREN];
    uint32_t child_count;

    /* Text */
    char *text;
} silverui_node_impl_t;

static silverui_node_impl_t g_nodes[SILVERUI_MAX_NODES];
static silverui_node_t g_root = 0;
static int g_root_w = 0;
static int g_root_h = 0;

static silverui_node_impl_t *node_get(silverui_node_t id) {
    if (id == 0 || id >= SILVERUI_MAX_NODES) return NULL;
    if (!g_nodes[id].used) return NULL;
    return &g_nodes[id];
}

static silverui_node_t node_alloc(silverui_node_type_t type) {
    for (silverui_node_t i = 1; i < SILVERUI_MAX_NODES; i++) {
        if (!g_nodes[i].used) {
            memset(&g_nodes[i], 0, sizeof(g_nodes[i]));
            g_nodes[i].used = true;
            g_nodes[i].type = type;
            g_nodes[i].style.x = 0;
            g_nodes[i].style.y = 0;
            g_nodes[i].style.w = -1;
            g_nodes[i].style.h = -1;
            g_nodes[i].style.padding = 0;
            g_nodes[i].style.bg = 0;
            g_nodes[i].style.fg = COLOR_WHITE;
            g_nodes[i].style.has_bg = false;
            g_nodes[i].style.has_fg = true;
            return i;
        }
    }
    return 0;
}

static void node_free_text(silverui_node_impl_t *n) {
    if (n->text) {
        kfree(n->text);
        n->text = NULL;
    }
}

void silverui_init(int root_w, int root_h) {
    g_root_w = root_w;
    g_root_h = root_h;
    silverui_reset();
}

void silverui_reset(void) {
    for (int i = 0; i < SILVERUI_MAX_NODES; i++) {
        if (g_nodes[i].used) {
            node_free_text(&g_nodes[i]);
        }
    }
    memset(g_nodes, 0, sizeof(g_nodes));
    g_root = 0;
}

silverui_node_t silverui_create_view(void) {
    return node_alloc(SILVERUI_NODE_VIEW);
}

silverui_node_t silverui_create_text(const char *text) {
    silverui_node_t id = node_alloc(SILVERUI_NODE_TEXT);
    silverui_set_text(id, text ? text : "");
    return id;
}

void silverui_set_text(silverui_node_t node, const char *text) {
    silverui_node_impl_t *n = node_get(node);
    if (!n || n->type != SILVERUI_NODE_TEXT) return;

    node_free_text(n);
    size_t len = strlen(text ? text : "");
    n->text = (char *)kmalloc(len + 1);
    if (!n->text) return;
    memcpy(n->text, text ? text : "", len);
    n->text[len] = 0;
}

void silverui_set_style(silverui_node_t node, const silverui_style_t *style) {
    silverui_node_impl_t *n = node_get(node);
    if (!n || !style) return;
    n->style = *style;
}

bool silverui_append_child(silverui_node_t parent, silverui_node_t child) {
    silverui_node_impl_t *p = node_get(parent);
    silverui_node_impl_t *c = node_get(child);
    if (!p || !c) return false;
    if (p->child_count >= SILVERUI_MAX_CHILDREN) return false;
    if (child == parent) return false;

    /* Detach from old parent */
    if (c->parent) {
        silverui_node_impl_t *op = node_get(c->parent);
        if (op) {
            for (uint32_t i = 0; i < op->child_count; i++) {
                if (op->children[i] == child) {
                    for (uint32_t j = i; j + 1 < op->child_count; j++) op->children[j] = op->children[j + 1];
                    op->child_count--;
                    break;
                }
            }
        }
    }

    c->parent = parent;
    p->children[p->child_count++] = child;
    return true;
}

void silverui_clear_children(silverui_node_t parent) {
    silverui_node_impl_t *p = node_get(parent);
    if (!p) return;
    for (uint32_t i = 0; i < p->child_count; i++) {
        silverui_node_impl_t *c = node_get(p->children[i]);
        if (c) c->parent = 0;
    }
    p->child_count = 0;
}

void silverui_set_root(silverui_node_t node) {
    if (!node_get(node)) return;
    g_root = node;
}

static int text_measure_w(const char *s) {
    if (!s) return 0;
    return (int)strlen(s) * FONT_WIDTH;
}

static int text_measure_h(void) {
    return FONT_HEIGHT;
}

static void layout_node(silverui_node_t id, int x, int y, int w, int h) {
    silverui_node_impl_t *n = node_get(id);
    if (!n) return;

    int nx = x + n->style.x;
    int ny = y + n->style.y;
    int nw = (n->style.w >= 0) ? n->style.w : w;
    int nh = (n->style.h >= 0) ? n->style.h : h;

    if (nw < 0) nw = 0;
    if (nh < 0) nh = 0;

    n->lx = nx;
    n->ly = ny;
    n->lw = nw;
    n->lh = nh;

    int pad = n->style.padding;
    int cx = nx + pad;
    int cy = ny + pad;
    int cw = nw - pad * 2;
    int ch = nh - pad * 2;
    if (cw < 0) cw = 0;
    if (ch < 0) ch = 0;

    if (n->type == SILVERUI_NODE_TEXT) {
        int tw = text_measure_w(n->text);
        int th = text_measure_h();
        n->lw = (n->style.w >= 0) ? nw : tw + pad * 2;
        n->lh = (n->style.h >= 0) ? nh : th + pad * 2;
        return;
    }

    /* Column layout: children stacked vertically */
    int cur_y = cy;
    for (uint32_t i = 0; i < n->child_count; i++) {
        silverui_node_impl_t *c = node_get(n->children[i]);
        if (!c) continue;

        /* First pass: if child has explicit h, use it; else infer for text; else give remaining */
        int child_h = (c->style.h >= 0) ? c->style.h : -1;
        if (child_h < 0 && c->type == SILVERUI_NODE_TEXT) child_h = text_measure_h() + c->style.padding * 2;
        if (child_h < 0) child_h = 0;

        layout_node(n->children[i], cx, cur_y, cw, child_h);
        silverui_node_impl_t *cl = node_get(n->children[i]);
        if (cl) cur_y += cl->lh;
    }
}

void silverui_layout(void) {
    if (!g_root) return;
    layout_node(g_root, 0, 0, g_root_w, g_root_h);
}

static void paint_node(silverui_node_t id) {
    silverui_node_impl_t *n = node_get(id);
    if (!n) return;

    if (n->style.has_bg) {
        fb_fill_rect(n->lx, n->ly, n->lw, n->lh, n->style.bg);
    }

    if (n->type == SILVERUI_NODE_TEXT) {
        int pad = n->style.padding;
        uint32_t fg = n->style.has_fg ? n->style.fg : COLOR_WHITE;
        font_draw_string(n->lx + pad, n->ly + pad, n->text ? n->text : "", fg);
        return;
    }

    for (uint32_t i = 0; i < n->child_count; i++) {
        paint_node(n->children[i]);
    }
}

void silverui_paint(void) {
    if (!g_root) return;
    paint_node(g_root);
    fb_swap_buffers();
}

