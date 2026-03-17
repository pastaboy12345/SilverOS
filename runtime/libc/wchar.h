#ifndef SILVEROS_WCHAR_H
#define SILVEROS_WCHAR_H

#include <stddef.h>

typedef int wint_t;
typedef struct { int dummy; } mbstate_t;

#ifdef __cplusplus
extern "C" {
#endif

// Minimal stubs for STL
size_t wcslen(const wchar_t *s);

#ifdef __cplusplus
}
#endif

#endif
