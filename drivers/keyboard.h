#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "../cpu/type.h"

#include <stdbool.h>


#define BACKSPACE 0x0E
#define ENTER 0x1C
#define CTRL 0x1D
#define SHIFT 0x2A
#define CAP_LOCK 0x3A
#define DEL 0x53

#define ARROW_RIGHT 0x4D
#define ARROW_LEFT 0x4B
#define ARROW_UP 0x48
#define ARROW_DOWN 0x50

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

void keyboard_buffer_push(char scancode);
char keyboard_buffer_pop();

uint8_t keyboard_get_ascii_from_scancode(uint8_t scancode);

#endif