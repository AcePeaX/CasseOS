#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "../cpu/type.h"

#include <stdbool.h>

// Structure to store key states for modifier keys
typedef struct {
    bool ctrl;
    bool shift;
    bool alt;
    bool caps_lock;
    bool num_lock;
    bool scroll_lock;
} KeyState;

extern KeyState key_state;  // Initialize all states to false


void init_keyboard();
void set_keyboard_type(uint8_t type);

uint8_t keyboard_get_ascii_from_scancode(uint8_t scancode);

#endif