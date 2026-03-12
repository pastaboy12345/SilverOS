#include "../include/pmm.h"
#include "../include/string.h"
#include "../include/serial.h"

#define MAX_PAGES (1024 * 1024)  /* Support up to 4 GB (4GB / 4KB) */

static uint8_t page_bitmap[MAX_PAGES / 8];
static uint64_t total_pages = 0;
static uint64_t free_pages = 0;

static inline void bitmap_set(uint64_t page) {
    page_bitmap[page / 8] |= (1 << (page % 8));
}

static inline void bitmap_clear(uint64_t page) {
    page_bitmap[page / 8] &= ~(1 << (page % 8));
}

static inline bool bitmap_test(uint64_t page) {
    return page_bitmap[page / 8] & (1 << (page % 8));
}

void pmm_init(uint64_t mem_size, struct multiboot2_tag_mmap *mmap_tag, uint64_t kernel_start, uint64_t kernel_end) {
    total_pages = 0;
    free_pages = 0;

    /* Initially mark all pages as used/reserved */
    memset(page_bitmap, 0xFF, sizeof(page_bitmap));

    if (!mmap_tag) {
        serial_printf("[PMM] WARNING: No memory map provided, falling back to basic info\n");
        total_pages = mem_size / PAGE_SIZE;
        if (total_pages > MAX_PAGES) total_pages = MAX_PAGES;
        
        uint64_t start_page = (kernel_end + PAGE_SIZE - 1) / PAGE_SIZE;
        if (start_page < 512) start_page = 512;
        
        for (uint64_t i = start_page; i < total_pages; i++) {
            bitmap_clear(i);
            free_pages++;
        }
    } else {
        serial_printf("[PMM] Using Multiboot2 memory map\n");
        struct multiboot2_mmap_entry *entry;
        for (entry = mmap_tag->entries;
             (uint8_t *)entry < (uint8_t *)mmap_tag + mmap_tag->size;
             entry = (struct multiboot2_mmap_entry *)((uintptr_t)entry + mmap_tag->entry_size)) {
            
            serial_printf("[PMM] Region: 0x%p - 0x%p, type=%d\n",
                          entry->addr, entry->addr + entry->len, entry->type);

            if (entry->type != MULTIBOOT2_MEMORY_AVAILABLE) continue;

            for (uint64_t addr = entry->addr; addr < entry->addr + entry->len; addr += PAGE_SIZE) {
                uint64_t page = addr / PAGE_SIZE;
                if (page >= MAX_PAGES) break;

                /* Skip first 2MB (kernel + boot structures) */
                if (addr < kernel_end || addr < 0x200000) continue;

                if (page > total_pages) total_pages = page + 1;
                bitmap_clear(page);
                free_pages++;
            }
        }
    }

    serial_printf("[PMM] Initialized: %d total pages tracked, %d free pages (%d MB free)\n",
                  total_pages, free_pages, (free_pages * PAGE_SIZE) / (1024 * 1024));
}

void *pmm_alloc_page(void) {
    for (uint64_t i = 0; i < total_pages; i++) {
        if (!bitmap_test(i)) {
            bitmap_set(i);
            free_pages--;
            void *addr = (void *)(i * PAGE_SIZE);
            memset(addr, 0, PAGE_SIZE);
            return addr;
        }
    }
    serial_printf("[PMM] ERROR: Out of physical memory!\n");
    return NULL;
}

void pmm_free_page(void *addr) {
    uint64_t page = (uint64_t)addr / PAGE_SIZE;
    if (page < total_pages && bitmap_test(page)) {
        bitmap_clear(page);
        free_pages++;
    }
}

uint64_t pmm_get_free_pages(void) {
    return free_pages;
}
