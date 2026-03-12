#include "../include/mouse.h"
#include "../include/idt.h"
#include "../include/pic.h"
#include "../include/io.h"
#include "../include/serial.h"
#include "../include/framebuffer.h"

static mouse_state_t mouse = {0};
static uint8_t mouse_cycle = 0;
static int8_t mouse_bytes[3];
static bool prev_left = false;

static void mouse_wait_write(void) {
    int timeout = 100000;
    while (timeout--) {
        if ((inb(0x64) & 0x02) == 0) return;
    }
}

static void mouse_wait_read(void) {
    int timeout = 100000;
    while (timeout--) {
        if ((inb(0x64) & 0x01) != 0) return;
    }
}

static void mouse_write(uint8_t data) {
    mouse_wait_write();
    outb(0x64, 0xD4);
    mouse_wait_write();
    outb(0x60, data);
}

static uint8_t mouse_read(void) {
    mouse_wait_read();
    return inb(0x60);
}

static void mouse_handler(struct interrupt_frame *frame) {
    (void)frame;

    uint8_t status = inb(0x64);
    if (!(status & 0x20)) {
        pic_send_eoi(12);
        return;
    }

    uint8_t data = inb(0x60);

    switch (mouse_cycle) {
        case 0:
            mouse_bytes[0] = data;
            if (data & 0x08) mouse_cycle++;  /* Bit 3 must be set in first byte */
            break;
        case 1:
            mouse_bytes[1] = data;
            mouse_cycle++;
            break;
        case 2:
            mouse_bytes[2] = data;
            mouse_cycle = 0;

            /* Parse packet */
            int dx = mouse_bytes[1];
            int dy = mouse_bytes[2];

            /* Sign extend */
            if (mouse_bytes[0] & 0x10) dx |= 0xFFFFFF00;
            if (mouse_bytes[0] & 0x20) dy |= 0xFFFFFF00;

            /* Update position */
            mouse.x += dx;
            mouse.y -= dy;  /* Y is inverted in PS/2 */

            /* Clamp to screen */
            if (mouse.x < 0) mouse.x = 0;
            if (mouse.y < 0) mouse.y = 0;
            if (mouse.x >= (int)fb.width)  mouse.x = fb.width - 1;
            if (mouse.y >= (int)fb.height) mouse.y = fb.height - 1;

            /* Buttons */
            bool new_left = mouse_bytes[0] & 0x01;
            mouse.right_button  = mouse_bytes[0] & 0x02;
            mouse.middle_button = mouse_bytes[0] & 0x04;

            if (new_left && !prev_left)  mouse.left_clicked = true;
            if (!new_left && prev_left)  mouse.left_released = true;
            mouse.left_button = new_left;
            prev_left = new_left;

            break;
    }

    pic_send_eoi(12);
}

void mouse_init(void) {
    /* Enable auxiliary mouse device */
    mouse_wait_write();
    outb(0x64, 0xA8);

    /* Enable interrupts */
    mouse_wait_write();
    outb(0x64, 0x20);
    mouse_wait_read();
    uint8_t status = inb(0x60) | 0x02;
    mouse_wait_write();
    outb(0x64, 0x60);
    mouse_wait_write();
    outb(0x60, status);

    /* Use default settings */
    mouse_write(0xF6);
    mouse_read();  /* ACK */

    /* Enable data reporting */
    mouse_write(0xF4);
    mouse_read();  /* ACK */

    /* Set initial position to center */
    mouse.x = fb.width / 2;
    mouse.y = fb.height / 2;

    idt_set_handler(44, mouse_handler);
    pic_clear_mask(12);  /* Unmask IRQ12 */
    pic_clear_mask(2);   /* Unmask cascade IRQ2 */

    serial_printf("[MOUSE] PS/2 mouse initialized at (%d, %d)\n",
                  (uint64_t)mouse.x, (uint64_t)mouse.y);
}

void mouse_get_state(mouse_state_t *state) {
    *state = mouse;
}

void mouse_clear_clicks(void) {
    mouse.left_clicked = false;
    mouse.left_released = false;
}
