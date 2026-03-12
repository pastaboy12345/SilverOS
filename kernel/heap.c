#include "../include/heap.h"
#include "../include/string.h"
#include "../include/serial.h"

/* Simple linked-list heap allocator */

struct heap_block {
    size_t             size;
    bool               free;
    struct heap_block  *next;
};

#define BLOCK_HEADER_SIZE sizeof(struct heap_block)
#define ALIGN_UP(x, a)   (((x) + (a) - 1) & ~((a) - 1))

static struct heap_block *heap_start = NULL;
static void              *heap_end = NULL;

void heap_init(void *start, size_t size) {
    heap_start = (struct heap_block *)start;
    heap_end   = (void *)((uint8_t *)start + size);

    heap_start->size = size - BLOCK_HEADER_SIZE;
    heap_start->free = true;
    heap_start->next = NULL;

    serial_printf("[HEAP] Initialized at 0x%x, size=%d KB\n",
                  (uint64_t)start, (uint64_t)(size / 1024));
}

static struct heap_block *find_free_block(size_t size) {
    struct heap_block *block = heap_start;
    while (block) {
        if (block->free && block->size >= size) {
            return block;
        }
        block = block->next;
    }
    return NULL;
}

static void split_block(struct heap_block *block, size_t size) {
    if (block->size >= size + BLOCK_HEADER_SIZE + 64) {
        struct heap_block *new_block = (struct heap_block *)((uint8_t *)block + BLOCK_HEADER_SIZE + size);
        new_block->size = block->size - size - BLOCK_HEADER_SIZE;
        new_block->free = true;
        new_block->next = block->next;

        block->size = size;
        block->next = new_block;
    }
}

void *kmalloc(size_t size) {
    if (size == 0) return NULL;

    size = ALIGN_UP(size, 16);

    struct heap_block *block = find_free_block(size);
    if (!block) {
        serial_printf("[HEAP] ERROR: Out of heap memory! Requested %d bytes\n", (uint64_t)size);
        return NULL;
    }

    split_block(block, size);
    block->free = false;

    return (void *)((uint8_t *)block + BLOCK_HEADER_SIZE);
}

void *kcalloc(size_t count, size_t size) {
    size_t total = count * size;
    void *ptr = kmalloc(total);
    if (ptr) memset(ptr, 0, total);
    return ptr;
}

void kfree(void *ptr) {
    if (!ptr) return;

    struct heap_block *block = (struct heap_block *)((uint8_t *)ptr - BLOCK_HEADER_SIZE);
    block->free = true;

    /* Coalesce with next block if free */
    if (block->next && block->next->free) {
        block->size += BLOCK_HEADER_SIZE + block->next->size;
        block->next = block->next->next;
    }

    /* Coalesce with previous block if free */
    struct heap_block *prev = heap_start;
    while (prev && prev->next != block) {
        prev = prev->next;
    }
    if (prev && prev->free) {
        prev->size += BLOCK_HEADER_SIZE + block->size;
        prev->next = block->next;
    }
}

void *krealloc(void *ptr, size_t new_size) {
    if (!ptr) return kmalloc(new_size);
    if (new_size == 0) { kfree(ptr); return NULL; }

    struct heap_block *block = (struct heap_block *)((uint8_t *)ptr - BLOCK_HEADER_SIZE);
    if (block->size >= new_size) return ptr;

    void *new_ptr = kmalloc(new_size);
    if (new_ptr) {
        memcpy(new_ptr, ptr, block->size);
        kfree(ptr);
    }
    return new_ptr;
}
