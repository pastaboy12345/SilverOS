; GDT and IDT flush routines for x86_64

section .text
bits 64

; void gdt_flush(uint64_t gdt_ptr) — rdi = pointer to GDT descriptor
global gdt_flush
gdt_flush:
    lgdt [rdi]
    ; Reload CS via far return
    mov ax, 0x10        ; kernel data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    ; Far jump to reload CS
    push 0x08           ; kernel code segment
    lea rax, [rel .flush_done]
    push rax
    retfq
.flush_done:
    ret

; void idt_flush(uint64_t idt_ptr) — rdi = pointer to IDT descriptor
global idt_flush
idt_flush:
    lidt [rdi]
    ret
