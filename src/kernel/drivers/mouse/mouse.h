#ifndef MOUSE_H
#define MOUSE_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    int32_t x;
    int32_t y;
    uint8_t buttons; // Bit 0: Left, Bit 1: Right, Bit 2: Middle
} mouse_state_t;

void mouse_initialize(void);
bool mouse_isr(void);
void mouse_get_state(mouse_state_t *state);

#endif