#include "../include/rtl8139.h"
#include "../include/io.h"
#include "../include/pci.h"
#include "../include/console.h"
#include "../include/idt.h"
#include "../include/pic.h"

#define RTL_VENDOR_ID 0x10EC
#define RTL_DEVICE_ID 0x8139

static uint32_t io_base = 0;
uint8_t rtl8139_mac[6];

/* We align buffers to 4K. VRAM map = Physical Map in this kernel, so virtual addresses are physical. */
static uint8_t rx_buffer[8192 + 16 + 1500] __attribute__((aligned(4096)));
static uint8_t tx_buffers[4][1536] __attribute__((aligned(4096)));

static uint8_t tx_cur = 0;
static uint16_t rx_cur = 0;

extern void net_receive_packet(void *data, uint16_t len);

static void rtl8139_handler(struct interrupt_frame *frame) {
    (void)frame;
    uint16_t status = inw(io_base + 0x3E);
    
    if (status & 0x01) { // RX OK
        while ((inb(io_base + 0x37) & 0x01) == 0) { // Buffer not empty
            uint16_t *rx_data = (uint16_t *)(rx_buffer + rx_cur);
            uint16_t rx_status = rx_data[0];
            uint16_t rx_length = rx_data[1];
            
            if (!(rx_status & 0x01)) { // RX OK bit off
                break;
            }
            
            void *packet = (void *)(rx_buffer + rx_cur + 4);
            uint16_t packet_len = rx_length - 4; // Length includes Ethernet CRC (4 bytes)
            
            // Forward physical packet to Networking Layer
            // Uncomment once net_receive_packet is implemented
            // net_receive_packet(packet, packet_len);
            
            rx_cur = (rx_cur + rx_length + 4 + 3) & ~3; // Align to 4 bytes
            if (rx_cur >= 8192) {
                rx_cur -= 8192;
            }
            outw(io_base + 0x38, rx_cur - 16);
        }
    }
    
    // Clear Interrupt Status
    outw(io_base + 0x3E, 0x05); // RX OK & TX OK
    
    // Ack PIC
    // Interrupt line could be e.g. 11. 
    pic_send_eoi(frame->int_no - 32);
}

void rtl8139_send_packet(void *data, uint32_t len) {
    if (!io_base) return;
    
    // Copy data to tx buffer
    uint8_t *tx_buf = tx_buffers[tx_cur];
    for (uint32_t i = 0; i < len; i++) {
        tx_buf[i] = ((uint8_t*)data)[i];
    }
    
    // Write physical address to TSD0..3
    outl(io_base + 0x20 + (tx_cur * 4), (uint32_t)(uintptr_t)tx_buf);
    
    // Write size to TSD0..3 (also triggers send)
    // Bit 13: own bit, low starts transfer. 
    outl(io_base + 0x10 + (tx_cur * 4), len);
    
    tx_cur = (tx_cur + 1) % 4;
}

void rtl8139_init(void) {
    uint8_t bus, slot, func;
    if (!pci_find_device(RTL_VENDOR_ID, RTL_DEVICE_ID, &bus, &slot, &func)) {
        kprintf("[RTL8139] Device not found on PCI bus.\n");
        return;
    }
    
    uint32_t bar0 = pci_get_bar(bus, slot, func, 0);
    if (!(bar0 & 1)) {
        kprintf("[RTL8139] BAR0 is not an I/O port mapping.\n");
        return;
    }
    
    io_base = bar0 & ~3;
    kprintf("[RTL8139] Found at bus %d, slot %d, func %d, I/O base 0x%x\n", bus, slot, func, io_base);
    
    pci_enable_bus_mastering(bus, slot, func);
    
    // Power on
    outb(io_base + 0x52, 0x0);
    
    // Software reset
    outb(io_base + 0x37, 0x10);
    while ((inb(io_base + 0x37) & 0x10) != 0) { }
    
    // Get MAC Address
    for (int i = 0; i < 6; i++) {
        rtl8139_mac[i] = inb(io_base + i);
    }
    kprintf("[RTL8139] MAC Address: %02x:%02x:%02x:%02x:%02x:%02x\n",
                  rtl8139_mac[0], rtl8139_mac[1], rtl8139_mac[2],
                  rtl8139_mac[3], rtl8139_mac[4], rtl8139_mac[5]);
                  
    // Init RX Buffer
    outl(io_base + 0x30, (uint32_t)(uintptr_t)rx_buffer);
    
    // TOK and ROK mask in IMR
    outw(io_base + 0x3C, 0x0005);
    
    // Configure Receive (RCR) -> AB, AM, APM, AAP (0xF), WRAP (0x80)
    outl(io_base + 0x44, 0x8F);
    
    // Enable Receive & Transmit
    outb(io_base + 0x37, 0x0C);
    
    // Setup IRQ Hook
    uint8_t irq = pci_get_interrupt_line(bus, slot, func);
    if (irq != 0xFF && irq != 0) {
        kprintf("[RTL8139] Hooking IRQ %d\n", irq);
        idt_set_handler(32 + irq, rtl8139_handler);
        pic_clear_mask(irq);
    }
    
    kprintf("[RTL8139] Initialized.\n");
}
