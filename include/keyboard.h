#ifndef SILVEROS_KEYBOARD_H
#define SILVEROS_KEYBOARD_H

#include "types.h"

#define KEY_BUFFER_SIZE 256

/* Special key codes (above ASCII range) */
#define KEY_ESCAPE    0x1B
#define KEY_BACKSPACE 0x08
#define KEY_TAB       0x09
#define KEY_ENTER     0x0A
#define KEY_LSHIFT    0x80
#define KEY_RSHIFT    0x81
#define KEY_LCTRL     0x82
#define KEY_LALT      0x83
#define KEY_CAPSLOCK  0x84
#define KEY_F1        0x85
#define KEY_F2        0x86
#define KEY_F3        0x87
#define KEY_F4        0x88
#define KEY_UP        0x90
#define KEY_DOWN      0x91
#define KEY_LEFT      0x92
#define KEY_RIGHT     0x93
#define KEY_HOME      0x94
#define KEY_END       0x95
#define KEY_PAGEUP    0x96
#define KEY_PAGEDOWN  0x97
#define KEY_DELETE    0x98

void    keyboard_init(void);
char    keyboard_getchar(void);       /* Blocking */
int     keyboard_haschar(void);       /* Non-blocking check */
char    keyboard_getchar_nb(void);    /* Non-blocking, returns 0 if empty */
bool    keyboard_shift_pressed(void);
bool    keyboard_ctrl_pressed(void);

#endif
