#ifndef DRIVERS_KEYBOARD_KEYBOARD_H
#define DRIVERS_KEYBOARD_KEYBOARD_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ----- Public key constants (match your old code where possible) ----- */
#define KB_BACKSPACE   0x0E   /* PS/2 set1 scancode for backspace (press) */
#define KB_ENTER       0x1C
#define KB_LSHIFT      0x2A
#define KB_RSHIFT      0x36
#define KB_LCTRL       0x1D
#define KB_CAPSLOCK    0x3A
#define KB_LALT        0x38

#define KB_ARROW_RIGHT 0x4D
#define KB_ARROW_LEFT 0x4B
#define KB_ARROW_UP 0x48
#define KB_ARROW_DOWN 0x50
/* These are symbolic — USB path uses HID usage IDs; we convert to ASCII here. */

typedef struct {
    uint8_t pressed;   /* 1=keydown, 0=keyup */
    uint8_t ascii;     /* 0 if non-printable */
    uint16_t code;     /* PS/2 scancode or HID usage (for debugging) */
    uint8_t modifiers; /* bit0=Ctrl, bit1=Shift, bit2=Alt, bit3=Caps */
} KeyEvent;

/* ----- Subsystem init / config ----- */
void keyboard_subsystem_init(void);

/* Keyboard layout: 0 = QWERTY, 1 = AZERTY (matches your old set) */
void keyboard_set_layout(uint8_t layout_id);

/* Auto echo to screen toggle (if you had that behavior elsewhere) */
void keyboard_set_auto_display(uint8_t on);

/* ----- Producer-side APIs (drivers feed events here) ----- */

/* PS/2 ISR should call this with raw set1 scancode (make/break). */
void keyboard_on_ps2_scancode(uint8_t scancode);

/* Helpers for managing USB keyboards */
int  keyboard_register_usb_boot_keyboard(uint8_t address,
                                            uint8_t endpoint_addr,
                                            uint8_t interval_ms,
                                            uint16_t wMaxPacketSize);

/* Enable/disable a registered keyboard without freeing its slot */
void keyboard_usb_set_enabled(int dev_index, bool enabled);
bool keyboard_usb_is_enabled(int dev_index);
void keyboard_usb_unregister(int dev_index);

/* Feed 8-byte HID Boot report from UHCI */
void keyboard_usb_on_boot_report(int dev_index, const uint8_t report[8]);


/* Optionally clear/unregister all (e.g., on bus reset). */
void keyboard_unregister_all(void);

/* ----- Consumer-side APIs (TTY / shell reads) ----- */

/* Non-blocking: 1 if a char is available to pop, else 0. */
int  keyboard_has_char(void);

/* Pop next ASCII char (returns 0 if none). Backspace and Enter preserved. */
char keyboard_read_char(void);

/* Optional: read a richer event (returns 0 if none). */
int  keyboard_read_event(KeyEvent *ev);

#ifdef __cplusplus
}
#endif
#endif /* DRIVERS_KEYBOARD_KEYBOARD_H */
