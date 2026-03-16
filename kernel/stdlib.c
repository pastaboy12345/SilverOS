#include "../include/stdlib.h"
#include "../include/heap.h"
#include "../include/serial.h"
#include "../include/io.h"

void *malloc(size_t size) {
    return kmalloc(size);
}

void *calloc(size_t count, size_t size) {
    return kcalloc(count, size);
}

void *realloc(void *ptr, size_t new_size) {
    return krealloc(ptr, new_size);
}

void free(void *ptr) {
    kfree(ptr);
}

void abort(void) {
    serial_printf("[KERNEL] ABORT called\n");
    cli();
    while (1) hlt();
}

void exit(int status) {
    serial_printf("[KERNEL] EXIT called with status %d\n", status);
    cli();
    while (1) hlt();
}
