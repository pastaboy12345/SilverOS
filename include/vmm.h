#ifndef SILVEROS_VMM_H
#define SILVEROS_VMM_H

#include "types.h"

/* Page table flags */
#define VMM_PRESENT  (1ULL << 0)
#define VMM_WRITABLE (1ULL << 1)
#define VMM_USER     (1ULL << 2)
#define VMM_HUGE     (1ULL << 7)
#define VMM_NX       (1ULL << 63)

/* POSIX-like memory protection flags */
#define PROT_NONE  0x00
#define PROT_READ  0x01
#define PROT_WRITE 0x02
#define PROT_EXEC  0x04

/* MAP flags */
#define MAP_PRIVATE   0x02
#define MAP_ANONYMOUS 0x20
#define MAP_FIXED     0x10

void vmm_init(void);
void vmm_map(uint64_t virt, uint64_t phys, uint64_t flags);
void vmm_unmap(uint64_t virt);
void vmm_set_flags(uint64_t virt, uint64_t flags);

void *vmm_mmap(void *addr, size_t length, int prot, int flags);
int   vmm_mprotect(void *addr, size_t len, int prot);

#endif
