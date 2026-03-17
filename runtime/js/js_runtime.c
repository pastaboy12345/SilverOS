#include "js_runtime.h"
#include "elk.h"
#include "../platform/platform.h"
#include "../../include/serial.h"
#include "../libc/stdio.h"
#include "../../include/heap.h"
#include "../../include/string.h"

#define JS_MEM_SIZE (128 * 1024)

static struct js *js_ctx = NULL;
static void *js_mem = NULL;

static jsval_t js_print(struct js *js, jsval_t *args, int nargs) {
    for (int i = 0; i < nargs; i++) {
        const char *s = js_str(js, args[i]);
        printf("%s%s", s, i == nargs - 1 ? "" : " ");
    }
    printf("\n");
    return js_mkundef();
}

void js_runtime_init(void) {
    serial_printf("[JS] Initializing...\n");
    js_mem = kmalloc(JS_MEM_SIZE);
    if (!js_mem) {
        serial_printf("[JS] Failed to allocate memory\n");
        return;
    }
    
    js_ctx = js_create(js_mem, JS_MEM_SIZE);
    if (!js_ctx) {
        serial_printf("[JS] Failed to create context\n");
        return;
    }
    
    /* Register built-ins */
    js_set(js_ctx, js_glob(js_ctx), "print", js_mkfun(js_print));
    
    serial_printf("[JS] Runtime initialized (%d KB)\n", JS_MEM_SIZE / 1024);
}


void js_runtime_eval(const char *code) {
    if (!js_ctx) return;
    jsval_t res = js_eval(js_ctx, code, strlen(code));
    if (js_type(res) == JS_ERR) {
        printf("[JS] Error: %s\n", js_str(js_ctx, res));
    }
}
