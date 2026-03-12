#include "../include/keyboard.h"
#include "../include/idt.h"
#include "../include/pic.h"
#include "../include/io.h"
#include "../include/serial.h"

static char key_buffer[KEY_BUFFER_SIZE];
static volatile int buf_head = 0;
static volatile int buf_tail = 0;

static bool shift_held = false;
static bool ctrl_held = false;
static bool caps_lock = false;

/* US QWERTY scancode set 1 to ASCII (lowercase) */
static const char scancode_to_ascii[] = {
    0, KEY_ESCAPE, '1','2','3','4','5','6','7','8','9','0','-','=', KEY_BACKSPACE,
    KEY_TAB, 'q','w','e','r','t','y','u','i','o','p','[',']', KEY_ENTER,
    KEY_LCTRL, 'a','s','d','f','g','h','j','k','l',';','\'', '`',
    KEY_LSHIFT, '\\','z','x','c','v','b','n','m',',','.','/', KEY_RSHIFT,
    '*', KEY_LALT, ' ', KEY_CAPSLOCK,
    KEY_F1, KEY_F2, KEY_F3, KEY_F4, 0, 0, 0, 0, 0, 0, /* F5-F10 */
    0, 0, /* Num Lock, Scroll Lock */
    KEY_HOME, KEY_UP, KEY_PAGEUP, '-', KEY_LEFT, 0, KEY_RIGHT, '+',
    KEY_END, KEY_DOWN, KEY_PAGEDOWN, 0, KEY_DELETE
};

/* Shift version */
static const char scancode_to_ascii_shift[] = {
    0, KEY_ESCAPE, '!','@','#','$','%','^','&','*','(',')','_','+', KEY_BACKSPACE,
    KEY_TAB, 'Q','W','E','R','T','Y','U','I','O','P','{','}', KEY_ENTER,
    KEY_LCTRL, 'A','S','D','F','G','H','J','K','L',':','"', '~',
    KEY_LSHIFT, '|','Z','X','C','V','B','N','M','<','>','?', KEY_RSHIFT,
    '*', KEY_LALT, ' ', KEY_CAPSLOCK
};

static void buf_put(char c) {
    int next = (buf_head + 1) % KEY_BUFFER_SIZE;
    if (next != buf_tail) {
        key_buffer[buf_head] = c;
        buf_head = next;
    }
}

static void keyboard_handler(struct interrupt_frame *frame) {
    (void)frame;
    uint8_t scancode = inb(0x60);

    /* Key release (bit 7 set) */
    if (scancode & 0x80) {
        uint8_t released = scancode & 0x7F;
        if (released == 0x2A || released == 0x36) shift_held = false;
        if (released == 0x1D) ctrl_held = false;
        pic_send_eoi(1);
        return;
    }

    /* Key press */
    if (scancode == 0x2A || scancode == 0x36) {
        shift_held = true;
        pic_send_eoi(1);
        return;
    }
    if (scancode == 0x1D) {
        ctrl_held = true;
        pic_send_eoi(1);
        return;
    }
    if (scancode == 0x3A) {
        caps_lock = !caps_lock;
        pic_send_eoi(1);
        return;
    }

    if (scancode < sizeof(scancode_to_ascii)) {
        char c;
        bool use_shift = shift_held;

        /* Caps lock affects letters */
        if (scancode >= 0x10 && scancode <= 0x19) use_shift = shift_held ^ caps_lock; /* q-p */
        if (scancode >= 0x1E && scancode <= 0x26) use_shift = shift_held ^ caps_lock; /* a-l */
        if (scancode >= 0x2C && scancode <= 0x32) use_shift = shift_held ^ caps_lock; /* z-m */

        if (use_shift && scancode < sizeof(scancode_to_ascii_shift)) {
            c = scancode_to_ascii_shift[scancode];
        } else {
            c = scancode_to_ascii[scancode];
        }

        if (c) buf_put(c);
    }

    pic_send_eoi(1);
}

void keyboard_init(void) {
    idt_set_handler(33, keyboard_handler);
    pic_clear_mask(1);  /* Unmask IRQ1 */
    serial_printf("[KEYBOARD] PS/2 keyboard initialized\n");
}

int keyboard_haschar(void) {
    return buf_head != buf_tail;
}

char keyboard_getchar(void) {
    while (!keyboard_haschar()) {
        __asm__ volatile ("hlt");
    }
    char c = key_buffer[buf_tail];
    buf_tail = (buf_tail + 1) % KEY_BUFFER_SIZE;
    return c;
}

char keyboard_getchar_nb(void) {
    if (!keyboard_haschar()) return 0;
    char c = key_buffer[buf_tail];
    buf_tail = (buf_tail + 1) % KEY_BUFFER_SIZE;
    return c;
}

bool keyboard_shift_pressed(void) { return shift_held; }
bool keyboard_ctrl_pressed(void) { return ctrl_held; }
