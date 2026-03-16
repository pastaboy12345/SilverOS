#ifndef SILVEROS_STDLIB_H
#define SILVEROS_STDLIB_H

#include "types.h"

void *malloc(size_t size);
void *calloc(size_t count, size_t size);
void *realloc(void *ptr, size_t new_size);
void  free(void *ptr);

void  abort(void);
void  exit(int status);

#endif
