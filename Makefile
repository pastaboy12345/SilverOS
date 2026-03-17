# ============================================================================
# SilverOS — Makefile
# x86_64 bare-metal OS built with NASM + GCC + GNU LD
# ============================================================================

# Toolchain
ASM      = nasm
CC       = gcc
LD       = ld
CXX      = g++
GRUB_MK  = grub-mkrescue

# Flags
ASMFLAGS = -f elf64
CFLAGS   = -ffreestanding -mno-red-zone -DENABLE_V8=$(ENABLE_V8) -DENABLE_DESKTOP=$(ENABLE_DESKTOP) \
           -mcmodel=kernel -Wall -Wextra -O2 -I include -std=gnu11 \
           -fno-stack-protector -fno-pic -fno-pie -c
CXXFLAGS = -ffreestanding -mno-red-zone -DENABLE_V8=$(ENABLE_V8) -DENABLE_DESKTOP=$(ENABLE_DESKTOP) -fno-exceptions -fno-rtti \
           -mcmodel=kernel -Wall -Wextra -O2 -I runtime/js/stl -I include -I /mnt/dev/v8/include \
           -std=gnu++20 -fno-stack-protector -fno-pic -fno-pie -c
LDFLAGS  = -T boot/linker.ld -nostdlib -z max-page-size=0x1000

# Feature toggles
ENABLE_V8 ?= 0
ENABLE_DESKTOP ?= 1

# Directories
BUILD    = build
ISO_DIR  = $(BUILD)/iso
ULTRA_SDK = third_party/ultralight/sdk
ULTRA_BIN = $(abspath $(ULTRA_SDK))/bin
ULTRA_CAPTURE_TOOL = $(BUILD)/ultralight_capture_frame
ULTRA_CAPTURE_HEADER = runtime/ultralight/generated_ultralight_frame.h

# Source files
ASM_SOURCES = boot/boot.asm kernel/isr.asm kernel/gdt_idt_flush.asm
C_SOURCES   = kernel/kernel.c kernel/string.c kernel/serial.c \
              kernel/gdt.c kernel/idt.c kernel/pic.c kernel/timer.c \
              kernel/pmm.c kernel/heap.c kernel/vmm.c kernel/user.c kernel/pci.c kernel/random.c kernel/stdlib.c \
              drivers/framebuffer.c drivers/font.c drivers/keyboard.c \
              drivers/console.c drivers/mouse.c \
              drivers/ata.c drivers/rtc.c drivers/rtl8139.c \
              runtime/platform/platform.c \
    runtime/libc/stdio.c runtime/libc/errno.c \
    runtime/libc/sys/mman.c \
              net/net.c net/ethernet.c net/arp.c net/ipv4.c net/icmp.c \
              fs/silverfs.c \
              desktop/desktop.c desktop/filebrowser.c \
              desktop/ultralight_app.c \
              
CPP_SOURCES = runtime/ultralight/ultralight_host.cpp
ifeq ($(ENABLE_V8),1)
CPP_SOURCES += runtime/js/v8_runtime.cpp runtime/js/v8_platform_silver.cpp
endif

# Object files
C_OBJECTS = $(C_SOURCES:%.c=$(BUILD)/%.o)
CPP_OBJECTS = $(CPP_SOURCES:%.cpp=$(BUILD)/%.o)
ASM_OBJECTS = $(ASM_SOURCES:%.asm=$(BUILD)/%.o)
OBJECTS = $(ASM_OBJECTS) $(C_OBJECTS) $(CPP_OBJECTS)

# Output
KERNEL   = $(BUILD)/silveros.bin
ISO      = $(BUILD)/silveros.iso
DISK     = $(BUILD)/disk.svd

# Host Tools
MKFS     = $(BUILD)/mkfs_svd
HOST_CC  = gcc
HOST_CXX = g++

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

$(ULTRA_CAPTURE_TOOL): tools/ultralight_capture_frame.cpp
	@mkdir -p $(BUILD)
	$(HOST_CXX) -O2 -std=gnu++20 \
		-I $(ULTRA_SDK)/include \
		-Wl,-rpath,$(ULTRA_BIN) \
		-L $(ULTRA_SDK)/bin \
		-o $@ $< \
		-lUltralight -lUltralightCore -lWebCore -lAppCore \
		-lpthread -ldl -lm
	@echo "==> Built host tool: $@"

$(ULTRA_CAPTURE_HEADER): $(ULTRA_CAPTURE_TOOL)
	LD_LIBRARY_PATH=$(ULTRA_BIN):$$LD_LIBRARY_PATH \
	$(ULTRA_CAPTURE_TOOL) $(abspath $@) $(abspath $(ULTRA_SDK))
	@echo "==> Generated Ultralight frame header: $@"

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

# C++ files
$(BUILD)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -o $@ $<
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -o $@ $<

$(BUILD)/runtime/ultralight/ultralight_host.o: $(ULTRA_CAPTURE_HEADER)


# ============================================================================
# Utility
# ============================================================================

# Verify multiboot2 header
check: $(KERNEL)
	grub-file --is-x86-multiboot2 $(KERNEL) && echo "Multiboot2 header: OK" || echo "Multiboot2 header: FAILED"
