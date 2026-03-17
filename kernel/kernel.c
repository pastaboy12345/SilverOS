/*
 * SilverOS — Kernel Main Entry Point
 * x86_64 bare-metal operating system
 */

#include "../include/types.h"
#include "../include/multiboot2.h"
#include "../include/serial.h"
#include "../include/string.h"
#include "../include/gdt.h"
#include "../include/idt.h"
#include "../include/pic.h"
#include "../include/timer.h"
#include "../include/pmm.h"
#include "../include/heap.h"
#include "../include/vmm.h"
#include "../runtime/js/js_runtime.h"
#include "../include/framebuffer.h"
#include "../include/keyboard.h"
#include "../include/keyboard.h"
#include "../include/mouse.h"
#include "../include/console.h"
#include "../include/desktop.h"
#include "../include/silverfs.h"
#include "../include/io.h"
#include "../include/ata.h"
#include "../include/rtc.h"
#include "../include/v8_runtime.h"

/* Implemented in `runtime/js/v8_platform_silver.cpp` */
extern void* create_silver_platform(void);

extern void (*_init_array_start []) (void);
extern void (*_init_array_end []) (void);

static void call_global_constructors(void) {
    for (void (**p) (void) = _init_array_start; p < _init_array_end; ++p) {
        (*p) ();
    }
}
#include "../include/pci.h"
#include "../include/rtl8139.h"
#include "../include/net.h"

#include "../include/user.h"
#include "../include/keyboard.h"

extern uint64_t _kernel_start;
extern uint64_t _kernel_end;

/* Heap area — 4 MB starting at 16 MB */
#define HEAP_START 0x1000000
#define HEAP_SIZE  (4 * 1024 * 1024)

void kernel_main(uint64_t multiboot_info_addr, uint64_t magic) {
    /* ---- Phase 1: Serial debug output ---- */
    serial_init();
    serial_printf("\n");
    serial_printf("=================================\n");
    serial_printf("  SilverOS v0.1.0 — Booting...\n");
    serial_printf("=================================\n");
    serial_printf("\n");

    /* Verify Multiboot2 */
    if ((uint32_t)magic != MULTIBOOT2_BOOTLOADER_MAGIC) {
        serial_printf("[BOOT] ERROR: Invalid Multiboot2 magic: 0x%x\n", magic);
        for (;;) hlt();
    }
    serial_printf("[BOOT] Multiboot2 magic OK: 0x%x\n", magic);
    serial_printf("[BOOT] Multiboot2 info at: 0x%p\n", multiboot_info_addr);

    /* ---- Phase 2: Parse Multiboot2 info ---- */
    struct multiboot2_info *mbi = (struct multiboot2_info *)multiboot_info_addr;
    struct multiboot2_tag_framebuffer *fb_tag = NULL;
    struct multiboot2_tag_basic_meminfo *mem_tag = NULL;
    struct multiboot2_tag_mmap *mmap_tag = NULL;

    if (mbi->total_size < 8) {
        serial_printf("[BOOT] ERROR: Multiboot2 info too small: %d bytes\n", mbi->total_size);
        for (;;) hlt();
    }

    struct multiboot2_tag *tag = (struct multiboot2_tag *)((uint8_t *)mbi + 8);
    while (tag->type != MULTIBOOT2_TAG_TYPE_END) {
        if (tag->size < 8) break;
        if (tag->type == MULTIBOOT2_TAG_TYPE_FRAMEBUFFER)
            fb_tag = (struct multiboot2_tag_framebuffer *)tag;
        else if (tag->type == MULTIBOOT2_TAG_TYPE_BASIC_MEMINFO)
            mem_tag = (struct multiboot2_tag_basic_meminfo *)tag;
        else if (tag->type == MULTIBOOT2_TAG_TYPE_MMAP)
            mmap_tag = (struct multiboot2_tag_mmap *)tag;
        
        tag = multiboot2_next_tag(tag);
        if ((uintptr_t)tag >= multiboot_info_addr + mbi->total_size) break;
    }

    /* ---- Phase 3: Early Graphics & Console ---- */
    if (fb_tag) {
        fb_init(fb_tag);
        fb_fill_screen(0x000000);
        console_init();
        console_clear();
        kprintf("SilverOS Kernel starting...\n");
        kprintf("Video: %dx%d %d-bpp\n", fb_tag->framebuffer_width, fb_tag->framebuffer_height, fb_tag->framebuffer_bpp);
    }


    /* ---- Phase 4: Hardware initialization ---- */
    kprintf("[INIT] GDT...");
    gdt_init();
    kprintf("OK\n[INIT] PIC...");
    pic_init();
    kprintf("OK\n[INIT] IDT...");
    idt_init();
    kprintf("OK\n");
    
    /* ---- Phase 5: Memory management ---- */
    uint64_t total_mem = 256 * 1024 * 1024;
    if (mem_tag) {
        total_mem = ((uint64_t)mem_tag->mem_upper + 1024) * 1024;
    }
    kprintf("[INIT] Memory: %u MB\n", (uint32_t)(total_mem / (1024 * 1024)));

    kprintf("[INIT] PMM...");
    pmm_init(total_mem, mmap_tag, (uint64_t)&_kernel_start, (uint64_t)&_kernel_end);
    kprintf("OK\n[INIT] HEAP...");
    heap_init((void *)HEAP_START, HEAP_SIZE);
    kprintf("OK\n[INIT] VMM...");
    vmm_init();
    kprintf("OK\n");

    /* Call C++ global constructors */
    kprintf("[INIT] C++ Runtime...");
    call_global_constructors();
    kprintf("OK\n");

    /* Verify C++ with a test call */
#if ENABLE_V8
    extern void v8_runtime_test(void);
    v8_runtime_test();
#endif

    /* ---- Phase 6: Input devices ---- */
    kprintf("[INIT] Keyboard...");
    keyboard_init();
    kprintf("OK\n");

    kprintf("[INIT] Mouse...");
    mouse_init();
    kprintf("OK\n");

    kprintf("[INIT] Enabling interrupts...");
    sti();
    kprintf("OK\n");
    
    /* Timer at 1000 Hz */
    kprintf("[INIT] Timer...");
    timer_init(1000);
    kprintf("OK\n");

    /* PCI Enumeration */
    kprintf("\n[INIT] Enumerating PCI Bus...\n");
    pci_scan_bus();

    /* Network Devices */
    rtl8139_init();
    net_init();

    /* ---- Phase 7: ATA & Filesystem ---- */
    kprintf("\n[INIT] Setting up Storage & SilverFS...\n");
    ata_init();
    if (silverfs_mount() != 0) {
        kprintf("[INIT] Formatting SilverFS (SVD disk not found or corrupt)...\n");
        silverfs_format();
        silverfs_mount();
    }

    /* ---- Phase 8: RTC Time ---- */
    rtc_init();
    rtc_time_t t;
    rtc_get_time(&t);
    kprintf("[RTC] Current hardware time: %d-%02d-%02d %02d:%02d:%02d\n",
                  t.year, t.month, t.day, t.hour, t.minute, t.second);

    /* ---- Phase 9: User Login Info ---- */
    user_init();
    kprintf("[INIT] User system ready.\n");

    /* ---- Phase JS: JS Runtime ---- */
#if ENABLE_V8 && !ENABLE_DESKTOP
    kprintf("[INIT] V8 Platform...");
    void* v8_platform = create_silver_platform();
    kprintf("OK (addr: %p)\n", v8_platform);

    kprintf("[INIT] V8 Runtime...");
    void* v8_runtime = create_v8_runtime();
    kprintf("OK\n");

    /* ---- Phase 10: JS Shell ---- */
    kprintf("[BOOT] Starting JS Shell...\n");
    
    const char* shell_src = 
        "println('SilverOS JS Shell Loaded');"
        "while(true) {"
        "  print('> ');"
        "  let line = '';"
        "  while(true) {"
        "    let ch = readChar();"
        "    if (ch == 10 || ch == 13) { println(''); break; }"
        "    let s = String.fromCharCode(ch);"
        "    line += s; print(s);"
        "  }"
        "  if (line == 'exit') break;"
        "  println('You typed: ' + line);"
        "}";

    v8_execute_script(v8_runtime, shell_src, "shell.js");
#endif

#if ENABLE_DESKTOP
    serial_printf("\n[INIT] Starting desktop shell...\n");
    desktop_init();
    desktop_run();
#endif

#if !ENABLE_DESKTOP
    serial_printf("\n[INIT] Starting minimal shell...\n");
    
    /* Set up full-screen console */
    console_init();
    console_clear();
    
    kprintf("SilverOS Kernel Shell\n");
    kprintf("Type 'help' for commands.\n\n");
    
    serial_printf("\n=================================\n  SilverOS Kernel Ready!\n=================================\n\n");
    
    char input[256];
    int pos = 0;
    console_puts("> ");
    
    while (1) {
        if (keyboard_haschar()) {
            char ch = keyboard_getchar_nb();
            if (ch == '\n') {
                input[pos] = '\0';
                console_puts("\n");
                
                if (pos > 0) {
                    if (strcmp(input, "help") == 0) {
                        console_puts("Commands: help, ls, clear, reboot\n");
                    } else if (strcmp(input, "clear") == 0) {
                        console_clear();
                    } else if (strcmp(input, "ls") == 0) {
                        silverfs_dirent_t entries[32];
                        int n = silverfs_readdir("/", entries, 32);
                        if (n <= 0) {
                            console_puts("Directory empty or error.\n");
                        } else {
                            for(int i=0; i<n; i++) {
                                console_puts(entries[i].name);
                                console_puts("  ");
                            }
                            console_puts("\n");
                        }
                    } else if (strcmp(input, "reboot") == 0) {
                        outb(0x64, 0xFE);
                    } else {
                        console_puts("Unknown command: ");
                        console_puts(input);
                        console_puts("\n");
                    }
                }
                pos = 0;
                console_puts("> ");
            } else if (ch == '\b') {
                if (pos > 0) {
                    pos--;
                    console_puts("\b \b");
                }
            } else if (ch >= 32 && ch < 127 && pos < 255) {
                input[pos++] = ch;
                char str[2] = {ch, 0};
                console_puts(str);
            }
        }
        fb_swap_buffers();
        hlt();
    }
#endif

    /* Should never reach here */
    serial_printf("[KERNEL] kernel_main returned — halting\n");
    for (;;) hlt();
}
