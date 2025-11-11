// drivers/keyboard/keyboard.h
#pragma once
#include <stdint.h>

// ---------- Sources ----------
typedef enum {
    KDEV_SOURCE_PS2 = 1,
    KDEV_SOURCE_USB = 2,
} kbd_source_t;

// ---------- Event types ----------
typedef enum {
    KEY_EV_PRESS   = 1,
    KEY_EV_RELEASE = 2,
    KEY_EV_REPEAT  = 3,
} key_event_type_t;

// ---------- Modifiers (bitmask in uint16_t) ----------
#define KM_SHIFT    (1u << 0)
#define KM_CTRL     (1u << 1)
#define KM_ALT      (1u << 2)
#define KM_GUI      (1u << 3)   // Win/Cmd

#define KM_RSHIFT   (1u << 4)
#define KM_RCTRL    (1u << 5)
#define KM_RALT     (1u << 6)
#define KM_RGUI     (1u << 7)

#define KM_CAPS     (1u << 8)
#define KM_NUM      (1u << 9)
#define KM_SCROLL   (1u << 10)

// ---------- Keycode type ----------
typedef uint16_t keycode_t;   // ASCII fits in 0x00..0x7F; extended start at 0x0100

// ASCII-compatible (use the real ASCII code for printables)
#define KC_NONE        ((keycode_t)0x0000)
#define KC_BACKSPACE   ((keycode_t)0x0008)
#define KC_TAB         ((keycode_t)0x0009)
#define KC_ENTER       ((keycode_t)0x000D)
#define KC_ESC         ((keycode_t)0x001B)
#define KC_SPACE       ((keycode_t)0x0020)

/* Extended keys (>= 0x0100) — stable across PS/2 & USB */
#define KC_BASE_EXT    ((keycode_t)0x0100)

#define KC_UP          (KC_BASE_EXT + 0x00)
#define KC_DOWN        (KC_BASE_EXT + 0x01)
#define KC_LEFT        (KC_BASE_EXT + 0x02)
#define KC_RIGHT       (KC_BASE_EXT + 0x03)
#define KC_HOME        (KC_BASE_EXT + 0x04)
#define KC_END         (KC_BASE_EXT + 0x05)
#define KC_PGUP        (KC_BASE_EXT + 0x06)
#define KC_PGDN        (KC_BASE_EXT + 0x07)
#define KC_INSERT      (KC_BASE_EXT + 0x08)
#define KC_DELETE      (KC_BASE_EXT + 0x09)

#define KC_F1          (KC_BASE_EXT + 0x20)
#define KC_F2          (KC_BASE_EXT + 0x21)
#define KC_F3          (KC_BASE_EXT + 0x22)
#define KC_F4          (KC_BASE_EXT + 0x23)
#define KC_F5          (KC_BASE_EXT + 0x24)
#define KC_F6          (KC_BASE_EXT + 0x25)
#define KC_F7          (KC_BASE_EXT + 0x26)
#define KC_F8          (KC_BASE_EXT + 0x27)
#define KC_F9          (KC_BASE_EXT + 0x28)
#define KC_F10         (KC_BASE_EXT + 0x29)
#define KC_F11         (KC_BASE_EXT + 0x2A)
#define KC_F12         (KC_BASE_EXT + 0x2B)

#define KC_CAPS_LOCK   (KC_BASE_EXT + 0x40)
#define KC_NUM_LOCK    (KC_BASE_EXT + 0x41)
#define KC_SCROLL_LOCK (KC_BASE_EXT + 0x42)

#define KC_LSHIFT      (KC_BASE_EXT + 0x50)
#define KC_RSHIFT      (KC_BASE_EXT + 0x51)
#define KC_LCTRL       (KC_BASE_EXT + 0x52)
#define KC_RCTRL       (KC_BASE_EXT + 0x53)
#define KC_LALT        (KC_BASE_EXT + 0x54)
#define KC_RALT        (KC_BASE_EXT + 0x55)
#define KC_LGUI        (KC_BASE_EXT + 0x56)
#define KC_RGUI        (KC_BASE_EXT + 0x57)

/* (Optional) Keypad */
#define KC_KP_0        (KC_BASE_EXT + 0x80)
#define KC_KP_1        (KC_BASE_EXT + 0x81)
#define KC_KP_2        (KC_BASE_EXT + 0x82)
#define KC_KP_3        (KC_BASE_EXT + 0x83)
#define KC_KP_4        (KC_BASE_EXT + 0x84)
#define KC_KP_5        (KC_BASE_EXT + 0x85)
#define KC_KP_6        (KC_BASE_EXT + 0x86)
#define KC_KP_7        (KC_BASE_EXT + 0x87)
#define KC_KP_8        (KC_BASE_EXT + 0x88)
#define KC_KP_9        (KC_BASE_EXT + 0x89)
#define KC_KP_DOT      (KC_BASE_EXT + 0x8A)
#define KC_KP_ENTER    (KC_BASE_EXT + 0x8B)
#define KC_KP_PLUS     (KC_BASE_EXT + 0x8C)
#define KC_KP_MINUS    (KC_BASE_EXT + 0x8D)
#define KC_KP_STAR     (KC_BASE_EXT + 0x8E)
#define KC_KP_SLASH    (KC_BASE_EXT + 0x8F)

// ---------- Event ----------
typedef struct {
    key_event_type_t type;   // PRESS/RELEASE/REPEAT
    keycode_t        code;   // ASCII or extended KC_*
    uint16_t         mods;   // KM_* mask (state after applying event)
    kbd_source_t     src;    // PS/2 or USB
    uint8_t          dev_id; // 0 for PS/2; USB logical id for USB devices
} key_event_t;

// ---------- Public API ----------
void kbd_dispatch_event(const key_event_t* ev);
uint16_t kbd_mods_state(void);
void kbd_set_lock_leds(uint8_t caps, uint8_t num, uint8_t scroll);

uint8_t kbd_register_device(kbd_source_t src, uint8_t hw_id);
void    kbd_unregister_device(uint8_t logical_id);

char    kbd_layout_ascii_from_set1(uint8_t scancode, uint16_t mods_state);

// ---- Lifecycle / options ----
void     kbd_subsystem_init(void);        // sets layout, clears buffers, etc.
void     kbd_set_layout(uint8_t layout);  // 0=QWERTY, 1=AZERTY (or what you prefer)
void     kbd_enable_auto_echo(int on);    // if on, echoes printable chars via a weak hook

// ---- Simple char I/O ----
int      kbd_has_char(void);              // non-blocking: >0 if a char is ready
char     kbd_read_char(void);             // non-blocking: returns 0 if none
char     kbd_getchar_blocking(void);      // blocking convenience

// ---- Raw event I/O ----
int      kbd_read_event(key_event_t* ev); // non-blocking: 1 ok, 0 none
