#include "keyboard.h"
#include "../screen.h"          /* if you still auto-echo somewhere */
#include "../../cpu/ports.h"
#include "../../libc/mem.h"
#include "../../libc/function.h"
#include "cpu/isr.h"
#include <string.h>

/* ---------- Small ring buffer of KeyEvent + ASCII queue ---------- */
#define KEYBUF_CAP 256

static KeyEvent ev_buf[KEYBUF_CAP];
static uint16_t ev_head = 0, ev_tail = 0;

static char ascii_buf[KEYBUF_CAP];
static uint16_t ascii_head = 0, ascii_tail = 0;

static uint8_t g_layout = 1;        /* 0=QWERTY, 1=AZERTY (default to your previous) */
static uint8_t g_auto_display = 0;

static uint8_t g_shift = 0;
static uint8_t g_ctrl  = 0;
static uint8_t g_alt   = 0;
static uint8_t g_caps  = 0;

/* Your old tables (kept intact) */
static const char sc_ascii_qwerty[] = {
    '?','?','1','2','3','4','5','6','7','8','9','0','-','=', '?','?', 'q','w','e','r','t','y',
    'u','i','o','p','[',']','?','?', 'a','s','d','f','g','h','j','k','l',';','\'','`','?','\\',
    'z','x','c','v','b','n','m',',','.','/','?','?','?',' '
};
static const char sc_ascii_qwerty_cap[] = {
    '?','?','1','2','3','4','5','6','7','8','9','0','-','=', '?','?', 'Q','W','E','R','T','Y',
    'U','I','O','P','[',']','?','?', 'A','S','D','F','G','H','J','K','L',';','\'','`','?','\\',
    'Z','X','C','V','B','N','M',',','.','/','?','?','?',' '
};

static const char sc_ascii_azerty[] = {
    '?','?','&',0x1A/*é*/,'"','\'','(','-',0x1A/*è*/,'_',0x1A/*ç*/,0x1A/*à*/,')','=', '?','?',
    'a','z','e','r','t','y','u','i','o','p','^','$','?','?', 'q','s','d','f','g','h','j','k','l',
    'm','?','*','?','*','w','x','c','v','b','n',',',';',';', '!', '!', '?','?',' '
};
static const char sc_ascii_azerty_cap[] = {
    '?','?','1','2','3','4','5','6','7','8','9','0',0x1A/*°*/,'+','?','?', 'A','Z','E','R','T','Y',
    'U','I','O','P',0x1A/*¨*/,0x1A/*£*/,'?','?', 'Q','S','D','F','G','H','J','K','L','M','?','*',
    '?','*','W','X','C','V','B','N','?', '.', '/', '?',0x1A/*§*/,'?','?',' '
};

/* Work pointers to current layout */
static const char *tbl_norm  = sc_ascii_azerty;
static const char *tbl_shift = sc_ascii_azerty_cap;

static inline void push_event(const KeyEvent *e) {
    uint16_t n = (ev_head + 1) % KEYBUF_CAP;
    if (n != ev_tail) { ev_buf[ev_head] = *e; ev_head = n; }
}

static inline void push_ascii(char c) {
    if (c == 0) return;
    uint16_t n = (ascii_head + 1) % KEYBUF_CAP;
    if (n != ascii_tail) { ascii_buf[ascii_head] = c; ascii_head = n; }
    if (g_auto_display) { screen_put(c); }
}

static inline int pop_event(KeyEvent *out) {
    if (ev_head == ev_tail) return 0;
    *out = ev_buf[ev_tail];
    ev_tail = (ev_tail + 1) % KEYBUF_CAP;
    return 1;
}

/* ----- Public API ----- */
void keyboard_subsystem_init(void) {
    keyboard_set_layout(g_layout);
    g_shift = g_ctrl = g_alt = g_caps = 0;
}

void keyboard_set_layout(uint8_t layout_id) {
    g_layout = layout_id ? 1 : 0;
    if (g_layout == 0) { tbl_norm = sc_ascii_qwerty; tbl_shift = sc_ascii_qwerty_cap; }
    else               { tbl_norm = sc_ascii_azerty; tbl_shift = sc_ascii_azerty_cap; }
}

void keyboard_set_auto_display(uint8_t on) { g_auto_display = on ? 1 : 0; }

int keyboard_has_char(void) { return ascii_head != ascii_tail; }

char keyboard_read_char(void) {
    if (ascii_head == ascii_tail) return 0;
    char c = ascii_buf[ascii_tail];
    ascii_tail = (ascii_tail + 1) % KEYBUF_CAP;
    return c;
}

int keyboard_read_event(KeyEvent *ev) { return pop_event(ev); }

void keyboard_unregister_all(void) {
    ev_head = ev_tail = ascii_head = ascii_tail = 0;
}

/* ----- PS/2 path: feed raw set1 scancodes and translate ----- */
static inline char translate_ps2_to_ascii(uint8_t scancode, int pressed) {
    uint8_t raw = scancode & 0x7F;
    if (!pressed) return 0;

    if (raw == KB_BACKSPACE) return '\b';
    if (raw == KB_ENTER)     return '\n';

    /* Bound check against our tables; your SC_MAX was 57; guard array access */
    if (raw >= 0 && raw < 0x40) {
        const char *tbl = (g_caps ^ g_shift) ? tbl_shift : tbl_norm;
        char c = tbl[raw];
        return c == '?' ? 0 : c;
    }
    return 0;
}

void keyboard_on_ps2_scancode(uint8_t scancode) {
    int pressed = ((scancode & 0x80) == 0); /* 1=make, 0=break */
    uint8_t raw = scancode & 0x7F;

    /* Update modifiers/toggles */
    if (raw == KB_CAPSLOCK && pressed) g_caps ^= 1;
    if (raw == KB_LSHIFT || raw == KB_RSHIFT) g_shift = pressed ? 1 : 0;
    if (raw == KB_LCTRL)  g_ctrl  = pressed ? 1 : 0;
    if (raw == KB_LALT)   g_alt   = pressed ? 1 : 0;

    KeyEvent e = {
        .pressed = pressed,
        .ascii   = translate_ps2_to_ascii(scancode, pressed),
        .code    = raw,
        .modifiers = (g_ctrl ? 1:0) | (g_shift?2:0) | (g_alt?4:0) | (g_caps?8:0)
    };
    push_event(&e);
    if (e.ascii) push_ascii(e.ascii);
}

static void keyboard_callback(registers_t *regs) {
    uint8_t sc = port_byte_in(0x60);
    keyboard_on_ps2_scancode(sc);
    UNUSED(regs);
}

/* ---- Glue targets for USB module ---- */
void keyboard_internal_push_event_alias(const KeyEvent* e) { push_event(e); }
void keyboard_internal_push_ascii_alias(char c) { push_ascii(c); }