#ifndef SILVEROS_SYS_MMAN_H
#define SILVEROS_SYS_MMAN_H

#include "../../../include/vmm.h"
#include "../stdint.h"
#include "../stddef.h"

#ifdef __cplusplus
extern "C" {
#endif

void *mmap(void *addr, size_t length, int prot, int flags, int fd, uint64_t offset);
int   mprotect(void *addr, size_t len, int prot);
int   munmap(void *addr, size_t length);

#ifdef __cplusplus
}
#endif

#endif
