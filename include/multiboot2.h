#ifndef SILVEROS_MULTIBOOT2_H
#define SILVEROS_MULTIBOOT2_H

#include "types.h"

/* Multiboot2 magic values */
#define MULTIBOOT2_HEADER_MAGIC     0xE85250D6
#define MULTIBOOT2_BOOTLOADER_MAGIC 0x36D76289

/* Multiboot2 tag types (boot information) */
#define MULTIBOOT2_TAG_TYPE_END             0
#define MULTIBOOT2_TAG_TYPE_CMDLINE         1
#define MULTIBOOT2_TAG_TYPE_BOOT_LOADER     2
#define MULTIBOOT2_TAG_TYPE_MODULE          3
#define MULTIBOOT2_TAG_TYPE_BASIC_MEMINFO   4
#define MULTIBOOT2_TAG_TYPE_BOOTDEV         5
#define MULTIBOOT2_TAG_TYPE_MMAP            6
#define MULTIBOOT2_TAG_TYPE_VBE             7
#define MULTIBOOT2_TAG_TYPE_FRAMEBUFFER     8
#define MULTIBOOT2_TAG_TYPE_ELF_SECTIONS    9
#define MULTIBOOT2_TAG_TYPE_APM             10
#define MULTIBOOT2_TAG_TYPE_EFI32           11
#define MULTIBOOT2_TAG_TYPE_EFI64           12
#define MULTIBOOT2_TAG_TYPE_SMBIOS          13
#define MULTIBOOT2_TAG_TYPE_ACPI_OLD        14
#define MULTIBOOT2_TAG_TYPE_ACPI_NEW        15
#define MULTIBOOT2_TAG_TYPE_NETWORK         16
#define MULTIBOOT2_TAG_TYPE_EFI_MMAP        17
#define MULTIBOOT2_TAG_TYPE_EFI_BS          18
#define MULTIBOOT2_TAG_TYPE_EFI32_IH        19
#define MULTIBOOT2_TAG_TYPE_EFI64_IH        20
#define MULTIBOOT2_TAG_TYPE_LOAD_BASE_ADDR  21

/* Memory map entry types */
#define MULTIBOOT2_MEMORY_AVAILABLE         1
#define MULTIBOOT2_MEMORY_RESERVED          2
#define MULTIBOOT2_MEMORY_ACPI_RECLAIMABLE  3
#define MULTIBOOT2_MEMORY_NVS               4
#define MULTIBOOT2_MEMORY_BADRAM            5

/* Framebuffer types */
#define MULTIBOOT2_FRAMEBUFFER_TYPE_INDEXED  0
#define MULTIBOOT2_FRAMEBUFFER_TYPE_RGB      1
#define MULTIBOOT2_FRAMEBUFFER_TYPE_EGA_TEXT 2

/* Boot information fixed header */
struct multiboot2_info {
    uint32_t total_size;
    uint32_t reserved;
} __attribute__((packed));

/* Generic tag header */
struct multiboot2_tag {
    uint32_t type;
    uint32_t size;
} __attribute__((packed));

/* Memory map entry */
struct multiboot2_mmap_entry {
    uint64_t addr;
    uint64_t len;
    uint32_t type;
    uint32_t zero;
} __attribute__((packed));

/* Memory map tag */
struct multiboot2_tag_mmap {
    uint32_t type;
    uint32_t size;
    uint32_t entry_size;
    uint32_t entry_version;
    struct multiboot2_mmap_entry entries[];
} __attribute__((packed));

/* Framebuffer tag */
struct multiboot2_tag_framebuffer {
    uint32_t type;
    uint32_t size;
    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t  framebuffer_bpp;
    uint8_t  framebuffer_type;
    uint8_t  reserved;
    /* Color info follows for RGB type */
    uint8_t  red_field_position;
    uint8_t  red_mask_size;
    uint8_t  green_field_position;
    uint8_t  green_mask_size;
    uint8_t  blue_field_position;
    uint8_t  blue_mask_size;
} __attribute__((packed));

/* Basic memory info tag */
struct multiboot2_tag_basic_meminfo {
    uint32_t type;
    uint32_t size;
    uint32_t mem_lower;
    uint32_t mem_upper;
} __attribute__((packed));

/* Command line tag */
struct multiboot2_tag_string {
    uint32_t type;
    uint32_t size;
    char     string[];
} __attribute__((packed));

/* Helper to iterate tags */
#define MULTIBOOT2_TAG_ALIGN 8
#define multiboot2_next_tag(tag) \
    ((struct multiboot2_tag *)((uint8_t *)(tag) + (((tag)->size + (MULTIBOOT2_TAG_ALIGN - 1)) & ~(MULTIBOOT2_TAG_ALIGN - 1))))

#endif
