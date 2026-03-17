; =============================================================================
; SilverOS Boot Assembly — Multiboot2 x86_64 Entry Point
; =============================================================================
; This file does the following:
;   1. Provides a Multiboot2 header (requesting 1024x768x32 framebuffer)
;   2. Sets up identity-mapped page tables for the first 4 GB
;   3. Enables long mode (64-bit) and PAE
;   4. Loads a 64-bit GDT
;   5. Jumps to kernel_main() in C

section .multiboot
align 8

; --- Multiboot2 Header ---
mb2_header_start:
    dd 0xE85250D6                   ; magic
    dd 0                            ; architecture: i386 (protected mode)
    dd mb2_header_end - mb2_header_start ; header length
    dd -(0xE85250D6 + 0 + (mb2_header_end - mb2_header_start)) ; checksum

    ; --- Framebuffer tag: request 1024x768x32 ---
    align 8
    dw 5                            ; type: framebuffer tag
    dw 0                            ; flags
    dd 20                           ; size
    dd 1024                         ; width
    dd 768                          ; height
    dd 32                           ; bpp

    ; --- End tag ---
    align 8
    dw 0                            ; type
    dw 0                            ; flags
    dd 8                            ; size
mb2_header_end:

; --- Stack ---
section .bss
align 16
stack_bottom:
    resb 65536                      ; 64 KB kernel stack
stack_top:

; --- Page tables (identity map first 4 GB with 2MB pages) ---
align 4096
pml4_table:
    resb 4096
pdpt_table:
    resb 4096
pd_table:
    resb 4096
pd_table2:
    resb 4096
pd_table3:
    resb 4096
pd_table4:
    resb 4096

section .text
bits 32

global _start
global pml4_table
extern kernel_main

_start:
    ; Save multiboot2 info pointer (in ebx) and magic (in eax)
    mov edi, ebx                    ; multiboot info pointer → edi (1st arg in SysV ABI)
    mov esi, eax                    ; magic → esi (2nd arg)

    ; Set up stack
    mov esp, stack_top

    ; --- Set up page tables ---
    ; PML4[0] → PDPT
    mov eax, pdpt_table
    or eax, 0x03                    ; present + writable
    mov [pml4_table], eax

    ; PDPT[0] → PD (first 1 GB)
    mov eax, pd_table
    or eax, 0x03
    mov [pdpt_table], eax

    ; PDPT[1] → PD2 (second 1 GB)
    mov eax, pd_table2
    or eax, 0x03
    mov [pdpt_table + 8], eax

    ; PDPT[2] → PD3 (third 1 GB)
    mov eax, pd_table3
    or eax, 0x03
    mov [pdpt_table + 16], eax

    ; PDPT[3] → PD4 (fourth 1 GB)
    mov eax, pd_table4
    or eax, 0x03
    mov [pdpt_table + 24], eax

    ; Fill each PD with 512 × 2MB pages = 1 GB per PD = 4 GB total
    ; PD table 1 (0 GB - 1 GB)
    mov ecx, 0
    mov ebx, pd_table
.fill_pd1:
    mov eax, ecx
    shl eax, 21                     ; eax = ecx * 2MB
    or eax, 0x83                    ; present + writable + huge (2MB page)
    mov [ebx + ecx * 8], eax
    inc ecx
    cmp ecx, 512
    jne .fill_pd1

    ; PD table 2 (1 GB - 2 GB)
    mov ecx, 0
    mov ebx, pd_table2
.fill_pd2:
    mov eax, ecx
    add eax, 512                    ; offset by 512 entries (1 GB)
    shl eax, 21
    or eax, 0x83
    mov [ebx + ecx * 8], eax
    inc ecx
    cmp ecx, 512
    jne .fill_pd2

    ; PD table 3 (2 GB - 3 GB)
    mov ecx, 0
    mov ebx, pd_table3
.fill_pd3:
    mov eax, ecx
    add eax, 1024
    shl eax, 21
    or eax, 0x83
    mov [ebx + ecx * 8], eax
    inc ecx
    cmp ecx, 512
    jne .fill_pd3

    ; PD table 4 (3 GB - 4 GB)
    mov ecx, 0
    mov ebx, pd_table4
.fill_pd4:
    mov eax, ecx
    add eax, 1536
    shl eax, 21
    or eax, 0x83
    mov [ebx + ecx * 8], eax
    inc ecx
    cmp ecx, 512
    jne .fill_pd4

    ; --- Enable PAE ---
    mov eax, cr4
    or eax, 1 << 5                  ; PAE bit
    mov cr4, eax

    ; --- Load PML4 into CR3 ---
    mov eax, pml4_table
    mov cr3, eax

    ; --- Enable Long Mode via EFER MSR ---
    mov ecx, 0xC0000080             ; IA32_EFER MSR
    rdmsr
    or eax, 1 << 8                  ; LME (Long Mode Enable)
    wrmsr

    ; --- Enable Paging ---
    mov eax, cr0
    or eax, 1 << 31                 ; PG bit
    mov cr0, eax

    ; --- Load 64-bit GDT and far-jump to 64-bit code ---
    lgdt [gdt64_pointer]
    jmp 0x08:long_mode_start        ; code segment selector = 0x08

; --- GDT for 64-bit mode ---
section .rodata
align 16
gdt64_start:
    dq 0                            ; null descriptor
gdt64_code:
    dq (1 << 43) | (1 << 44) | (1 << 47) | (1 << 53)  ; code segment: executable, code/data, present, 64-bit
gdt64_data:
    dq (1 << 44) | (1 << 47) | (1 << 41)               ; data segment: code/data, present, writable
gdt64_end:

gdt64_pointer:
    dw gdt64_end - gdt64_start - 1  ; limit
    dq gdt64_start                  ; base

; --- 64-bit code ---
section .text
bits 64

long_mode_start:
    ; Reload data segment registers
    mov ax, 0x10                    ; data segment selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Set up 64-bit stack (same stack, just zero upper bits)
    mov rsp, stack_top

    ; edi already has multiboot2 info pointer (zero-extended from 32-bit)
    ; esi already has multiboot2 magic
    ; These are the first two args in SysV x86-64 ABI

    ; Enable SSE
    mov rax, cr0
    and ax, 0xFFFB      ; Clear CR0.EM
    or ax, 0x0002       ; Set CR0.MP
    mov cr0, rax
    mov rax, cr4
    or ax, 3 << 9       ; Set CR4.OSFXSR and CR4.OSXMMEXCPT
    mov cr4, rax

    ; Jump to kernel
    call kernel_main

    ; Should never reach here
.halt:
    cli
    hlt
    jmp .halt
