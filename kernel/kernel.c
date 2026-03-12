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
#include "../include/framebuffer.h"
#include "../include/font.h"
#include "../include/keyboard.h"
#include "../include/mouse.h"
#include "../include/keyboard.h"
#include "../include/console.h"
#include "../include/bootanim.h"
#include "../include/silverfs.h"
#include "../include/desktop.h"
#include "../include/io.h"
#include "../include/ata.h"
#include "../include/rtc.h"
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
    kprintf("OK\n");

    /* ---- Phase 6: Input devices ---- */
    kprintf("[INIT] Keyboard...");
    keyboard_init();
    kprintf("OK\n[INIT] Mouse...");
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

    /* ---- Phase 9: Boot Loader Menu ---- */
    int selected = 1;
    int boot_choice = 0;
    while (boot_choice == 0) {
        fb_fill_rect(0, 0, fb.width, fb.height, RGB(20, 20, 30));
        font_draw_string(fb.width/2 - 70, fb.height/2 - 80, "SilverOS Boot Menu", RGB(220, 220, 240));

        if (selected == 1) {
            fb_fill_rect(fb.width/2 - 120, fb.height/2 - 30, 240, 24, RGB(50, 100, 200));
        }
        font_draw_string(fb.width/2 - 110, fb.height/2 - 26, "1. Start SilverOS Desktop", RGB(255, 255, 255));

        if (selected == 2) {
            fb_fill_rect(fb.width/2 - 120, fb.height/2 + 10, 240, 24, RGB(50, 100, 200));
        }
        font_draw_string(fb.width/2 - 110, fb.height/2 + 14, "2. Start Minimal Shell", RGB(255, 255, 255));

        font_draw_string(fb.width/2 - 90, fb.height/2 + 70, "Use Up/Down and Enter", RGB(150, 150, 150));

        fb_swap_buffers();

        if (keyboard_haschar()) {
            unsigned char key = (unsigned char)keyboard_getchar_nb();
            if (key == KEY_UP || key == KEY_DOWN) {
                selected = (selected == 1) ? 2 : 1;
            } else if (key == '\n' || key == KEY_ENTER) {
                boot_choice = selected;
            } else if (key == '1') {
                boot_choice = 1;
            } else if (key == '2') {
                boot_choice = 2;
            }
        } else {
            hlt();
        }
    }

    if (boot_choice == 1) {
        /* ---- Phase 10a: Desktop Mode ---- */
        bootanim_play();
        serial_printf("\n[INIT] Starting desktop environment...\n");
        console_init();
        
        /* Instead of opening desktop instantly, show login screen */
        desktop_init(); /* modified to handle login screen mode first */
        
        serial_printf("\n=================================\n  SilverOS Ready!\n=================================\n\n");
        desktop_run(); /* Never returns */
    } else {
        /* ---- Phase 10b: Shell Mode ---- */
        serial_printf("\n[INIT] Starting minimal shell...\n");
        fb_fill_rect(0, 0, fb.width, fb.height, 0); /* Clear screen */
        
        /* Set up full-screen console */
        console_init();
        console_clear();
        
        /* Simple Login Loop for Text Shell */
        bool logged_in = false;
        while (!logged_in) {
            console_puts("SilverOS Login\n");
            console_puts("Username: ");
            
            char username[32];
            int ulen = 0;
            while (1) {
                if (keyboard_haschar()) {
                    char ch = keyboard_getchar_nb();
                    if (ch == '\n') {
                        username[ulen] = '\0';
                        console_puts("\n");
                        break;
                    } else if (ch == '\b' && ulen > 0) {
                        ulen--;
                        console_puts("\b \b");
                    } else if (ch >= 32 && ch < 127 && ulen < 31) {
                        username[ulen++] = ch;
                        char str[2] = {ch, 0};
                        console_puts(str);
                    }
                }
                fb_swap_buffers();
                hlt();
            }
            
            console_puts("Password: ");
            char password[64];
            int plen = 0;
            while (1) {
                if (keyboard_haschar()) {
                    char ch = keyboard_getchar_nb();
                    if (ch == '\n') {
                        password[plen] = '\0';
                        console_puts("\n");
                        break;
                    } else if (ch == '\b' && plen > 0) {
                        plen--;
                        console_puts("\b \b"); /* Clear the asterisk */
                    } else if (ch >= 32 && ch < 127 && plen < 63) {
                        password[plen++] = ch;
                        console_puts("*"); /* Mask password */
                    }
                }
                fb_swap_buffers();
                hlt();
            }
            
            if (user_login(username, password)) {
                logged_in = true;
                console_puts("Login successful.\n\n");
            } else {
                console_puts("Login failed. Try again.\n\n");
            }
        }
        
        console_puts("SilverOS Minimal Shell\n");
        console_puts("Type 'help' for commands.\n\n");
        
        serial_printf("\n=================================\n  SilverOS Ready (Shell)!\n=================================\n\n");
        
        /* We'll loop here with a simple terminal */
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
                        /* Extremely basic builtin commands for shell mode */
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
    }

    /* Should never reach here */
    serial_printf("[KERNEL] kernel_main returned — halting\n");
    for (;;) hlt();
}
