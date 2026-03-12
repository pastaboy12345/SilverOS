#ifndef SILVEROS_IDT_H
#define SILVEROS_IDT_H

#include "types.h"

struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;       /* bits 0-2: IST, bits 3-7: reserved */
    uint8_t  type_attr; /* type and attributes */
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t reserved;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

/* Interrupt frame pushed by CPU + our stubs */
struct interrupt_frame {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t int_no, err_code;
    uint64_t rip, cs, rflags, rsp, ss;
} __attribute__((packed));

typedef void (*isr_handler_t)(struct interrupt_frame *frame);

void idt_init(void);
void idt_set_handler(uint8_t n, isr_handler_t handler);

#endif
