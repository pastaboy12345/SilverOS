#include "../include/pci.h"
#include "../include/io.h"
#include "../include/console.h"

uint32_t pci_read_config(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address;
    uint32_t lbus  = (uint32_t)bus;
    uint32_t lslot = (uint32_t)slot;
    uint32_t lfunc = (uint32_t)func;
    
    address = (uint32_t)((lbus << 16) | (lslot << 11) | (lfunc << 8) | (offset & 0xFC) | ((uint32_t)0x80000000));
    outl(PCI_CONFIG_ADDRESS, address);
    return inl(PCI_CONFIG_DATA);
}

void pci_write_config(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value) {
    uint32_t address;
    uint32_t lbus  = (uint32_t)bus;
    uint32_t lslot = (uint32_t)slot;
    uint32_t lfunc = (uint32_t)func;
    
    address = (uint32_t)((lbus << 16) | (lslot << 11) | (lfunc << 8) | (offset & 0xFC) | ((uint32_t)0x80000000));
    outl(PCI_CONFIG_ADDRESS, address);
    outl(PCI_CONFIG_DATA, value);
}

void pci_scan_bus(void) {
    kprintf("[PCI] Scanning bus 0...\n");
    for (int bus = 0; bus < 1; bus++) {
        for (int slot = 0; slot < 32; slot++) {
            uint32_t id_reg = pci_read_config(bus, slot, 0, 0);
            uint16_t vendor = id_reg & 0xFFFF;
            uint16_t device = (id_reg >> 16) & 0xFFFF;
            
            if (vendor != 0xFFFF) {
                kprintf("[PCI] Found device: bus %d, slot %d, vendor 0x%04x, device 0x%04x\n", 
                              bus, slot, (uint32_t)vendor, (uint32_t)device);
            }
        }
    }
}

int pci_find_device(uint16_t vendor_id, uint16_t device_id, uint8_t *bus_out, uint8_t *slot_out, uint8_t *func_out) {
    for (uint8_t bus = 0; bus < 2; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            for (uint8_t func = 0; func < 8; func++) {
                uint32_t id_reg = pci_read_config(bus, slot, func, 0);
                uint16_t vendor = id_reg & 0xFFFF;
                uint16_t device = (id_reg >> 16) & 0xFFFF;
                
                if (vendor == 0xFFFF) {
                    if (func == 0) break; // Device doesn't exist
                    continue;
                }
                
                if (vendor == vendor_id && device == device_id) {
                    if (bus_out) *bus_out = bus;
                    if (slot_out) *slot_out = slot;
                    if (func_out) *func_out = func;
                    return 1;
                }
            }
        }
    }
    return 0; // Not found
}

uint32_t pci_get_bar(uint8_t bus, uint8_t slot, uint8_t func, int bar) {
    uint8_t offset = 0x10 + (bar * 4);
    return pci_read_config(bus, slot, func, offset);
}

void pci_enable_bus_mastering(uint8_t bus, uint8_t slot, uint8_t func) {
    uint32_t command = pci_read_config(bus, slot, func, 0x04);
    command |= 0x0004; // Set bit 2 (Bus Master)
    pci_write_config(bus, slot, func, 0x04, command);
}

uint8_t pci_get_interrupt_line(uint8_t bus, uint8_t slot, uint8_t func) {
    uint32_t iline = pci_read_config(bus, slot, func, 0x3C);
    return iline & 0xFF;
}
