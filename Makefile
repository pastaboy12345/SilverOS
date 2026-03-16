# ============================================================================
# SilverOS — Makefile
# x86_64 bare-metal OS built with NASM + GCC + GNU LD
# ============================================================================

# Toolchain
ASM      = nasm
CC       = gcc
LD       = ld
GRUB_MK  = grub-mkrescue

# Flags
ASMFLAGS = -f elf64
CFLAGS   = -ffreestanding -mno-red-zone -mno-mmx -mno-sse -mno-sse2 \
           -mcmodel=kernel -Wall -Wextra -O2 -I include -std=gnu11 \
           -fno-stack-protector -fno-pic -fno-pie -c
LDFLAGS  = -T boot/linker.ld -nostdlib -z max-page-size=0x1000

# Directories
BUILD    = build
ISO_DIR  = $(BUILD)/iso

# Source files
ASM_SOURCES = boot/boot.asm kernel/isr.asm kernel/gdt_idt_flush.asm
C_SOURCES   = kernel/kernel.c kernel/string.c kernel/serial.c \
              kernel/gdt.c kernel/idt.c kernel/pic.c kernel/timer.c \
              kernel/pmm.c kernel/heap.c kernel/vmm.c kernel/user.c kernel/pci.c kernel/random.c kernel/stdlib.c \
              drivers/framebuffer.c drivers/font.c drivers/keyboard.c \
              drivers/console.c \
              drivers/ata.c drivers/rtc.c drivers/rtl8139.c \
              runtime/platform/platform.c runtime/libc/stdio.c \
              net/net.c net/ethernet.c net/arp.c net/ipv4.c net/icmp.c \
              fs/silverfs.c

# Object files
ASM_OBJECTS = $(patsubst %.asm, $(BUILD)/%.o, $(ASM_SOURCES))
C_OBJECTS   = $(patsubst %.c, $(BUILD)/%.o, $(C_SOURCES))
OBJECTS     = $(ASM_OBJECTS) $(C_OBJECTS)

# Output
KERNEL   = $(BUILD)/silveros.bin
ISO      = $(BUILD)/silveros.iso
DISK     = $(BUILD)/disk.svd

# Host Tools
MKFS     = $(BUILD)/mkfs_svd
HOST_CC  = gcc

# ============================================================================
# Targets
# ============================================================================

.PHONY: all clean run iso disk usb

all: $(KERNEL) $(DISK)

# Link kernel
$(KERNEL): $(OBJECTS)
	@mkdir -p $(BUILD)
	$(LD) $(LDFLAGS) -o $@ $^
	@echo "==> Kernel binary: $@"

# Build ISO
iso: $(KERNEL)
	@mkdir -p $(ISO_DIR)/boot/grub
	cp $(KERNEL) $(ISO_DIR)/boot/silveros.bin
	cp boot/grub.cfg $(ISO_DIR)/boot/grub/grub.cfg
	$(GRUB_MK) -o $(ISO) $(ISO_DIR)
	@echo "==> ISO image: $(ISO)"

# Build Host Tools
$(MKFS): tools/mkfs_svd.c
	@mkdir -p $(BUILD)
	$(HOST_CC) -O2 -o $@ $<
	@echo "==> Built host tool: $@"

# Create SVD Disk
$(DISK): $(MKFS)
	$(MKFS) $@
	@echo "==> Virtual Disk: $@"

disk: $(DISK)

# Run in QEMU
run: iso disk
	qemu-system-x86_64 \
		-cdrom $(ISO) \
		-hda $(DISK) \
		-m 256M \
		-serial stdio \
		-vga std \
		-nic user,model=rtl8139 \
		-enable-kvm 2>/dev/null || \
	qemu-system-x86_64 \
		-cdrom $(ISO) \
		-hda $(DISK) \
		-m 256M \
		-serial stdio \
		-vga std \
		-nic user,model=rtl8139

# Run without KVM (for systems without hardware virtualization)
run-nokvm: iso disk
	qemu-system-x86_64 \
		-cdrom $(ISO) \
		-hda $(DISK) \
		-m 256M \
		-serial stdio \
		-vga std \
		-nic user,model=rtl8139

# Flash USB
usb: iso disk
	@if [ -z "$(DEV)" ]; then \
		echo "Usage: make usb DEV=/dev/sdX"; \
		exit 1; \
	fi
	@echo "Flashing $(ISO) to $(DEV)..."
	sudo dd if=$(ISO) of=$(DEV) bs=1M status=progress
	@echo "Appending $(DISK) to $(DEV) at 64MB offset..."
	sudo dd if=$(DISK) of=$(DEV) bs=1M seek=64 status=progress
	sync

# Clean
clean:
	rm -rf $(BUILD)

# ============================================================================
# Compile rules
# ============================================================================

# Assembly files
$(BUILD)/%.o: %.asm
	@mkdir -p $(dir $@)
	$(ASM) $(ASMFLAGS) -o $@ $<

# C files
$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $<

# ============================================================================
# Utility
# ============================================================================

# Verify multiboot2 header
check: $(KERNEL)
	grub-file --is-x86-multiboot2 $(KERNEL) && echo "Multiboot2 header: OK" || echo "Multiboot2 header: FAILED"
