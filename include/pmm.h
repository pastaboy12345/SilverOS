#ifndef SILVEROS_PMM_H
#define SILVEROS_PMM_H

#include "types.h"
#include "multiboot2.h"

#define PAGE_SIZE 4096

void pmm_init(uint64_t mem_size, struct multiboot2_tag_mmap *mmap_tag, uint64_t kernel_start, uint64_t kernel_end);
void *pmm_alloc_page(void);
void pmm_free_page(void *addr);
uint64_t pmm_get_free_pages(void);

#endif
