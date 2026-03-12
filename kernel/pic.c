#include "../include/pic.h"
#include "../include/io.h"
#include "../include/serial.h"

void pic_init(void) {
    /* Save masks */
    uint8_t mask1 = inb(PIC1_DATA);
    uint8_t mask2 = inb(PIC2_DATA);

    /* Start initialization sequence (ICW1) */
    outb(PIC1_COMMAND, 0x11); io_wait();
    outb(PIC2_COMMAND, 0x11); io_wait();

    /* ICW2: remap IRQs — PIC1 to 0x20 (32), PIC2 to 0x28 (40) */
    outb(PIC1_DATA, 0x20); io_wait();
    outb(PIC2_DATA, 0x28); io_wait();

    /* ICW3: tell master PIC there is a slave at IRQ2, tell slave its cascade identity */
    outb(PIC1_DATA, 0x04); io_wait();
    outb(PIC2_DATA, 0x02); io_wait();

    /* ICW4: 8086/88 mode */
    outb(PIC1_DATA, 0x01); io_wait();
    outb(PIC2_DATA, 0x01); io_wait();

    /* Restore masks (all masked initially, we'll unmask as needed) */
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);

    (void)mask1;
    (void)mask2;

    serial_printf("[PIC] Remapped IRQs to 0x20-0x2F\n");
}

void pic_send_eoi(uint8_t irq) {
    if (irq >= 8) {
        outb(PIC2_COMMAND, 0x20);
    }
    outb(PIC1_COMMAND, 0x20);
}

void pic_set_mask(uint8_t irq) {
    uint16_t port;
    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }
    uint8_t value = inb(port) | (1 << irq);
    outb(port, value);
}

void pic_clear_mask(uint8_t irq) {
    uint16_t port;
    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }
    uint8_t value = inb(port) & ~(1 << irq);
    outb(port, value);
}
