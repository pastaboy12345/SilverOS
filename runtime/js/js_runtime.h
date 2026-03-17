#ifndef SILVEROS_JS_RUNTIME_H
#define SILVEROS_JS_RUNTIME_H

#include "../../include/types.h"

void js_runtime_init(void);
void js_runtime_eval(const char *code);

#endif
