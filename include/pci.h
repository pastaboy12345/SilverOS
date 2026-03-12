#ifndef SILVEROS_PCI_H
#define SILVEROS_PCI_H

#include "types.h"

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA    0xCFC

uint32_t pci_read_config(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
void pci_write_config(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value);

void pci_scan_bus(void);
int pci_find_device(uint16_t vendor_id, uint16_t device_id, uint8_t *bus_out, uint8_t *slot_out, uint8_t *func_out);

uint32_t pci_get_bar(uint8_t bus, uint8_t slot, uint8_t func, int bar);
void pci_enable_bus_mastering(uint8_t bus, uint8_t slot, uint8_t func);
uint8_t pci_get_interrupt_line(uint8_t bus, uint8_t slot, uint8_t func);

#endif
