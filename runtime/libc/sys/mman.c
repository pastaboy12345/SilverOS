#include "mman.h"
#include "../../../include/types.h"
#include "../../../include/vmm.h"

void *mmap(void *addr, size_t length, int prot, int flags, int fd, uint64_t offset) {
    (void)fd;
    (void)offset;
    return vmm_mmap(addr, length, prot, flags);
}

int mprotect(void *addr, size_t len, int prot) {
    return vmm_mprotect(addr, len, prot);
}

int munmap(void *addr, size_t length) {
    // Basic implementation: just unmap pages
    uint64_t vaddr = (uint64_t)addr;
    for (size_t i = 0; i < length; i += 4096) {
        vmm_unmap(vaddr + i);
    }
    return 0;
}
