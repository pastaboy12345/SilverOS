#ifndef SILVEROS_MOUSE_H
#define SILVEROS_MOUSE_H

#include "types.h"

typedef struct {
    int      x;
    int      y;
    bool     left_button;
    bool     right_button;
    bool     middle_button;
    bool     left_clicked;   /* One-shot on press */
    bool     left_released;  /* One-shot on release */
} mouse_state_t;

void mouse_init(void);
void mouse_get_state(mouse_state_t *state);
void mouse_clear_clicks(void);

#endif
