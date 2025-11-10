#include "keyboard.h"

// Minimal examples (extend as needed)
static keycode_t ps2_make_to_key(uint8_t code, int e0)
{
    if (!e0) {
        switch (code) {
            case 0x1C: return KC_ENTER;
            case 0x0E: return KC_BACKSPACE;
            case 0x0F: return KC_TAB;
            case 0x39: return KC_SPACE;
            case 0x2A: return KC_LSHIFT;
            case 0x36: return KC_RSHIFT;
            case 0x1D: return KC_LCTRL;
            case 0x38: return KC_LALT;
            case 0x3A: return KC_CAPS_LOCK;
            // letters/numbers: map to ASCII
            // e.g., 0x10='q', 0x11='w' … according to your layout table
        }
    } else { // E0-prefixed
        switch (code) {
            case 0x48: return KC_UP;
            case 0x50: return KC_DOWN;
            case 0x4B: return KC_LEFT;
            case 0x4D: return KC_RIGHT;
            case 0x47: return KC_HOME;
            case 0x4F: return KC_END;
            case 0x49: return KC_PGUP;
            case 0x51: return KC_PGDN;
            case 0x52: return KC_INSERT;
            case 0x53: return KC_DELETE;
            case 0x1D: return KC_RCTRL;
            case 0x38: return KC_RALT;
            // Windows keys often 0x5B/0x5C
        }
    }
    return KC_NONE;
}

// ISR-level byte stream -> high-level events
void ps2_on_scancode_byte(uint8_t b)
{
    static int e0 = 0, f0 = 0; // track 0xE0 (extended) and 0xF0 (break) if using Set2;
// For Set1: break is "make|0x80". If you’re truly on Set1, adapt accordingly.
// Example below assumes common PC Set1: 0xE0 prefix and break = make|0x80.

    if (b == 0xE0) { e0 = 1; return; }

    int is_break = (b & 0x80) != 0;
    uint8_t make  = b & 0x7F;

    keycode_t kc = ps2_make_to_key(make, e0);
    e0 = 0;

    if (kc == KC_NONE) return;

    // Update modifiers and locks
    key_event_t ev = {0};
    ev.src   = KDEV_SOURCE_PS2;
    ev.dev_id= 0; // PS/2 single port
    ev.code  = kc;
    ev.type  = is_break ? KEY_EV_RELEASE : KEY_EV_PRESS;

    // You’ll maintain a global mods state; here just dispatch:
    kbd_dispatch_event(&ev);
}
