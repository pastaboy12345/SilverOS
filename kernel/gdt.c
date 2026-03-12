#include "../include/gdt.h"
#include "../include/string.h"
#include "../include/serial.h"

/* 5 GDT entries + 1 TSS entry (TSS takes 2 GDT slots in 64-bit mode) */
static struct gdt_entry gdt_entries[7];
static struct gdt_ptr   gdt_pointer;
static struct tss_entry tss;

extern void gdt_flush(uint64_t gdt_ptr);

static void gdt_set_entry(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt_entries[num].base_low    = base & 0xFFFF;
    gdt_entries[num].base_middle = (base >> 16) & 0xFF;
    gdt_entries[num].base_high   = (base >> 24) & 0xFF;
    gdt_entries[num].limit_low   = limit & 0xFFFF;
    gdt_entries[num].granularity = ((limit >> 16) & 0x0F) | (gran & 0xF0);
    gdt_entries[num].access      = access;
}

void gdt_init(void) {
    memset(&tss, 0, sizeof(tss));
    tss.iomap_base = sizeof(tss);

    /* Null descriptor */
    gdt_set_entry(0, 0, 0, 0, 0);

    /* Kernel code segment: 0x08 */
    gdt_set_entry(1, 0, 0xFFFFF, 0x9A, 0xA0);  /* present, executable, readable, 64-bit */

    /* Kernel data segment: 0x10 */
    gdt_set_entry(2, 0, 0xFFFFF, 0x92, 0xC0);  /* present, writable */

    /* User code segment: 0x18 */
    gdt_set_entry(3, 0, 0xFFFFF, 0xFA, 0xA0);  /* present, ring 3, executable, readable, 64-bit */

    /* User data segment: 0x20 */
    gdt_set_entry(4, 0, 0xFFFFF, 0xF2, 0xC0);  /* present, ring 3, writable */

    /* TSS descriptor (64-bit TSS takes two GDT entries) */
    uint64_t tss_base = (uint64_t)&tss;
    uint32_t tss_limit = sizeof(tss) - 1;

    gdt_entries[5].limit_low   = tss_limit & 0xFFFF;
    gdt_entries[5].base_low    = tss_base & 0xFFFF;
    gdt_entries[5].base_middle = (tss_base >> 16) & 0xFF;
    gdt_entries[5].access      = 0x89;  /* present, 64-bit TSS available */
    gdt_entries[5].granularity = ((tss_limit >> 16) & 0x0F);
    gdt_entries[5].base_high   = (tss_base >> 24) & 0xFF;

    /* Upper 32 bits of TSS base in next entry */
    *((uint64_t *)&gdt_entries[6]) = (tss_base >> 32) & 0xFFFFFFFF;

    gdt_pointer.limit = sizeof(gdt_entries) - 1;
    gdt_pointer.base  = (uint64_t)&gdt_entries;

    gdt_flush((uint64_t)&gdt_pointer);

    /* Load TSS */
    __asm__ volatile ("mov $0x28, %%ax; ltr %%ax" ::: "ax");

    serial_printf("[GDT] Initialized with TSS\n");
}
