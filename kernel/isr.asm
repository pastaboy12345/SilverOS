; ISR and IRQ stub macros for x86_64
; These push the interrupt number and jump to a common handler

section .text
bits 64

extern isr_common_handler

; Macro for ISRs that don't push an error code — we push 0
%macro ISR_NOERR 1
global isr%1
isr%1:
    push 0              ; dummy error code
    push %1             ; interrupt number
    jmp isr_common_stub
%endmacro

; Macro for ISRs that push an error code (CPU does it)
%macro ISR_ERR 1
global isr%1
isr%1:
    push %1             ; interrupt number (error code already on stack)
    jmp isr_common_stub
%endmacro

; Macro for IRQs — mapped to ISR 32+
%macro IRQ 2
global irq%1
irq%1:
    push 0              ; dummy error code
    push %2             ; ISR number (32 + irq)
    jmp isr_common_stub
%endmacro

; Common stub: save all registers, call C handler, restore, iretq
isr_common_stub:
    ; Save all general-purpose registers
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    ; Pass stack pointer as argument (pointer to interrupt_frame)
    mov rdi, rsp
    call isr_common_handler

    ; Restore registers
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax

    ; Remove int_no and error code from stack
    add rsp, 16

    iretq

; --- Exception ISRs ---
ISR_NOERR 0    ; Division by zero
ISR_NOERR 1    ; Debug
ISR_NOERR 2    ; NMI
ISR_NOERR 3    ; Breakpoint
ISR_NOERR 4    ; Overflow
ISR_NOERR 5    ; Bound range exceeded
ISR_NOERR 6    ; Invalid opcode
ISR_NOERR 7    ; Device not available
ISR_ERR   8    ; Double fault
ISR_NOERR 9    ; Coprocessor segment overrun
ISR_ERR   10   ; Invalid TSS
ISR_ERR   11   ; Segment not present
ISR_ERR   12   ; Stack-segment fault
ISR_ERR   13   ; General protection fault
ISR_ERR   14   ; Page fault
ISR_NOERR 15   ; Reserved
ISR_NOERR 16   ; x87 floating-point
ISR_ERR   17   ; Alignment check
ISR_NOERR 18   ; Machine check
ISR_NOERR 19   ; SIMD floating-point
ISR_NOERR 20   ; Virtualization
ISR_NOERR 21   ; Reserved
ISR_NOERR 22   ; Reserved
ISR_NOERR 23   ; Reserved
ISR_NOERR 24   ; Reserved
ISR_NOERR 25   ; Reserved
ISR_NOERR 26   ; Reserved
ISR_NOERR 27   ; Reserved
ISR_NOERR 28   ; Reserved
ISR_NOERR 29   ; Reserved
ISR_ERR   30   ; Security exception
ISR_NOERR 31   ; Reserved

; --- Hardware IRQs ---
IRQ 0,  32     ; PIT timer
IRQ 1,  33     ; Keyboard
IRQ 2,  34     ; Cascade
IRQ 3,  35     ; COM2
IRQ 4,  36     ; COM1
IRQ 5,  37     ; LPT2
IRQ 6,  38     ; Floppy
IRQ 7,  39     ; Spurious (LPT1)
IRQ 8,  40     ; CMOS RTC
IRQ 9,  41     ; Free
IRQ 10, 42     ; Free
IRQ 11, 43     ; Free
IRQ 12, 44     ; PS/2 Mouse
IRQ 13, 45     ; FPU
IRQ 14, 46     ; Primary ATA
IRQ 15, 47     ; Secondary ATA
