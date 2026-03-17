#include "../include/vmm.h"
#include "../include/pmm.h"
#include "../include/string.h"
#include "../include/serial.h"

extern uint64_t pml4_table[];

static uint64_t *get_next_level(uint64_t *current_level, uint64_t entry_index, bool allocate) {
    if (current_level[entry_index] & VMM_PRESENT) {
        return (uint64_t *)(current_level[entry_index] & ~0xFFF);
    }

    if (!allocate) return NULL;

    uint64_t *new_level = pmm_alloc_page();
    if (!new_level) return NULL;

    memset(new_level, 0, 4096);
    current_level[entry_index] = (uint64_t)new_level | VMM_PRESENT | VMM_WRITABLE | VMM_USER;
    return new_level;
}

void vmm_init(void) {
    serial_printf("[VMM] Initialized using existing PML4 at 0x%p\n", pml4_table);
}

void vmm_map(uint64_t virt, uint64_t phys, uint64_t flags) {
    uint64_t pml4_idx = (virt >> 39) & 0x1FF;
    uint64_t pdpt_idx = (virt >> 30) & 0x1FF;
    uint64_t pd_idx   = (virt >> 21) & 0x1FF;
    uint64_t pt_idx   = (virt >> 12) & 0x1FF;

    uint64_t *pdpt = get_next_level(pml4_table, pml4_idx, true);
    uint64_t *pd   = get_next_level(pdpt, pdpt_idx, true);
    uint64_t *pt   = get_next_level(pd, pd_idx, true);

    pt[pt_idx] = (phys & ~0xFFF) | flags | VMM_PRESENT;
    
    /* Invalidate TLB */
    __asm__ volatile("invlpg (%0)" : : "r"(virt) : "memory");
}

void vmm_unmap(uint64_t virt) {
    uint64_t pml4_idx = (virt >> 39) & 0x1FF;
    uint64_t pdpt_idx = (virt >> 30) & 0x1FF;
    uint64_t pd_idx   = (virt >> 21) & 0x1FF;
    uint64_t pt_idx   = (virt >> 12) & 0x1FF;

    uint64_t *pdpt = get_next_level(pml4_table, pml4_idx, false);
    if (!pdpt) return;
    uint64_t *pd   = get_next_level(pdpt, pdpt_idx, false);
    if (!pd) return;
    uint64_t *pt   = get_next_level(pd, pd_idx, false);
    if (!pt) return;

    pt[pt_idx] = 0;
    __asm__ volatile("invlpg (%0)" : : "r"(virt) : "memory");
}

void vmm_set_flags(uint64_t virt, uint64_t flags) {
    uint64_t pml4_idx = (virt >> 39) & 0x1FF;
    uint64_t pdpt_idx = (virt >> 30) & 0x1FF;
    uint64_t pd_idx   = (virt >> 21) & 0x1FF;
    uint64_t pt_idx   = (virt >> 12) & 0x1FF;

    uint64_t *pdpt = get_next_level(pml4_table, pml4_idx, false);
    if (!pdpt) return;
    uint64_t *pd   = get_next_level(pdpt, pdpt_idx, false);
    if (!pd) return;
    uint64_t *pt   = get_next_level(pd, pd_idx, false);
    if (!pt) return;

    uint64_t phys = pt[pt_idx] & ~0xFFF;
    pt[pt_idx] = phys | flags | VMM_PRESENT;
    __asm__ volatile("invlpg (%0)" : : "r"(virt) : "memory");
}

static uint64_t next_vaddr = 0x40000000; // 1GB

void *vmm_mmap(void *addr, size_t length, int prot, int flags) {
    uint64_t vaddr = (uint64_t)addr;
    
    if (vaddr == 0) {
        vaddr = next_vaddr;
        next_vaddr += (length + 4095) & ~4095;
    }

    uint64_t vmm_flags = VMM_USER;
    if (prot & PROT_WRITE) vmm_flags |= VMM_WRITABLE;
    if (!(prot & PROT_EXEC)) vmm_flags |= VMM_NX;

    for (size_t i = 0; i < length; i += 4096) {
        uint64_t phys = (uint64_t)pmm_alloc_page();
        if (!phys) return NULL; // Should rollback on failure, but simplified for now
        vmm_map(vaddr + i, phys, vmm_flags);
    }

    return (void *)vaddr;
}

int vmm_mprotect(void *addr, size_t len, int prot) {
    uint64_t vaddr = (uint64_t)addr;
    uint64_t vmm_flags = VMM_USER;
    if (prot & PROT_WRITE) vmm_flags |= VMM_WRITABLE;
    if (!(prot & PROT_EXEC)) vmm_flags |= VMM_NX;

    for (size_t i = 0; i < len; i += 4096) {
        vmm_set_flags(vaddr + i, vmm_flags);
    }

    return 0;
}
