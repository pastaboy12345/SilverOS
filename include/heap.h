#ifndef SILVEROS_HEAP_H
#define SILVEROS_HEAP_H

#include "types.h"

void  heap_init(void *start, size_t size);
void *kmalloc(size_t size);
void *kcalloc(size_t count, size_t size);
void *krealloc(void *ptr, size_t new_size);
void  kfree(void *ptr);

#endif
