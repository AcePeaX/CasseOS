#include "keyboard.h"
#include "cpu/ports.h"
#include "cpu/isr.h"
#include "libc/mem.h"
#include "libc/function.h"
#include "libc/string.h"
#include <stdbool.h>

/* =========================
   Small ring buffers
   ========================= */
#define KEYBUF_CAP 256

static key_event_t ev_buf[KEYBUF_CAP];
static uint16_t ev_head = 0, ev_tail = 0;

static char ascii_buf[KEYBUF_CAP];
static uint16_t ascii_head = 0, ascii_tail = 0;

/* =========================
   Global keyboard state
   ========================= */
static uint8_t  g_layout = 1;        /* 0=QWERTY, 1=AZERTY (default like old code) */
static uint8_t  g_auto_echo = 0;

/* Modifiers/locks state (ONLY KM_* bits) */
static uint16_t g_mods = 0;

/* Track 0xE0 prefix for PS/2 set1 */
static uint8_t g_ps2_e0 = 0;

/* Logical device usage: dev0 is PS/2 if present, USB devices are 1..n */
static uint8_t g_ps2_registered = 0;
static uint8_t g_usb_next_id = 1;

/* =========================
   Old layout tables
   ========================= */
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

#define SC_ASCII_TABLE_LEN (sizeof(sc_ascii_qwerty))

/* Work pointers to current layout */
static const char *tbl_norm  = sc_ascii_azerty;
static const char *tbl_shift = sc_ascii_azerty_cap;

/* =========================
   Console echo hook (weak)
   ========================= */
__attribute__((weak)) void kb_console_putc(char c) {
    extern void kprint(const char* s);
    char b[2] = { c, 0 };
    kprint(b);
}

/* =========================
   Helpers: buffers
   ========================= */
static inline void push_event(const key_event_t *e) {
    uint16_t n = (uint16_t)((ev_head + 1) % KEYBUF_CAP);
    if (n != ev_tail) { ev_buf[ev_head] = *e; ev_head = n; }
}

static inline void push_ascii(char c) {
    if (c == 0) return;
    uint16_t n = (uint16_t)((ascii_head + 1) % KEYBUF_CAP);
    if (n != ascii_tail) { ascii_buf[ascii_head] = c; ascii_head = n; }
    if (g_auto_echo) { kb_console_putc(c); }
}

/* =========================
   Public API (from keyboard.h)
   ========================= */
void kbd_dispatch_event(const key_event_t* ev) {
    /* Copy so we can annotate mods as "state after event" */
    key_event_t e = *ev;

    /* Lock toggles: on PRESS of lock keys */
    if (e.type == KEY_EV_PRESS) {
        if (e.code == KC_CAPS_LOCK)   g_mods ^= KM_CAPS;
        if (e.code == KC_NUM_LOCK)    g_mods ^= KM_NUM;
        if (e.code == KC_SCROLL_LOCK) g_mods ^= KM_SCROLL;
    }

    /* Side modifiers -> update KM_* bits (do NOT mix KC_* into g_mods) */
    uint16_t side_set = 0, side_clr = 0, agg_set = 0, agg_clr = 0;
    switch (e.code) {
        case KC_LSHIFT:  side_set = KM_SHIFT;  agg_set = KM_SHIFT;  break;
        case KC_RSHIFT:  side_set = KM_RSHIFT; agg_set = KM_SHIFT;  break;
        case KC_LCTRL:   side_set = KM_CTRL;   agg_set = KM_CTRL;   break;
        case KC_RCTRL:   side_set = KM_RCTRL;  agg_set = KM_CTRL;   break;
        case KC_LALT:    side_set = KM_ALT;    agg_set = KM_ALT;    break;
        case KC_RALT:    side_set = KM_RALT;   agg_set = KM_ALT;    break;
        case KC_LGUI:    side_set = KM_GUI;    agg_set = KM_GUI;    break;
        case KC_RGUI:    side_set = KM_RGUI;   agg_set = KM_GUI;    break;
        default: break;
    }
    if (side_set || side_clr || agg_set || agg_clr) {
        if (e.type == KEY_EV_PRESS) {
            g_mods |= side_set;
            g_mods |= agg_set;
        } else if (e.type == KEY_EV_RELEASE) {
            /* Clear the side bit; then clear the aggregate if neither side is down */
            g_mods &= ~side_set;
            /* For simplicity we clear the aggregate too on release; if you later
               track both sides independently, compute agg from sides. */
            g_mods &= ~agg_set;
        }
    }

    /* mods snapshot AFTER applying this event */
    e.mods = g_mods;

    /* Enqueue event */
    push_event(&e);

    /* For PRESS of printable ASCII, also enqueue into char queue */
    if (e.type == KEY_EV_PRESS && e.code < 0x80) {
        push_ascii((char)e.code);
    }
}

uint16_t kbd_mods_state(void) { return g_mods; }

void kbd_set_lock_leds(uint8_t caps, uint8_t num, uint8_t scroll) {
    if (caps)   g_mods |= KM_CAPS;   else g_mods &= ~KM_CAPS;
    if (num)    g_mods |= KM_NUM;    else g_mods &= ~KM_NUM;
    if (scroll) g_mods |= KM_SCROLL; else g_mods &= ~KM_SCROLL;
    /* TODO: actually signal LEDs to PS/2 controller / USB HID OUT report */
}

/* Lifecycle / options */
void kbd_subsystem_init(void) {
    ev_head = ev_tail = ascii_head = ascii_tail = 0;
    g_mods = 0; g_ps2_e0 = 0;
    /* default layout */
    if (g_layout == 0) { tbl_norm = sc_ascii_qwerty; tbl_shift = sc_ascii_qwerty_cap; }
    else               { tbl_norm = sc_ascii_azerty; tbl_shift = sc_ascii_azerty_cap; }

    kbd_register_device(KDEV_SOURCE_PS2, 0);
}

void kbd_set_layout(uint8_t layout) {
    g_layout = layout ? 1 : 0;
    if (g_layout == 0) { tbl_norm = sc_ascii_qwerty; tbl_shift = sc_ascii_qwerty_cap; }
    else               { tbl_norm = sc_ascii_azerty; tbl_shift = sc_ascii_azerty_cap; }
}

void kbd_enable_auto_echo(int on) { g_auto_echo = on ? 1 : 0; }

/* Simple char I/O */
int  kbd_has_char(void) { return ascii_head != ascii_tail; }

char kbd_read_char(void) {
    if (ascii_head == ascii_tail) return 0;
    char c = ascii_buf[ascii_tail];
    ascii_tail = (uint16_t)((ascii_tail + 1) % KEYBUF_CAP);
    return c;
}

char kbd_getchar_blocking(void) {
    for (;;) {
        char c = kbd_read_char();
        if (c) return c;
        __asm__ __volatile__("hlt");
    }
}

/* Raw event I/O */
int  kbd_read_event(key_event_t *ev) {
    if (ev_head == ev_tail) return 0;
    *ev = ev_buf[ev_tail];
    ev_tail = (uint16_t)((ev_tail + 1) % KEYBUF_CAP);
    return 1;
}

/* =========================
   Device (de)registration
   ========================= */
static void ps2_irq1_handler(registers_t *r);

uint8_t kbd_register_device(kbd_source_t src, uint8_t hw_id) {
    (void)hw_id;
    if (src == KDEV_SOURCE_PS2) {
        if (g_ps2_registered) return 0;
        register_interrupt_handler(IRQ1, ps2_irq1_handler);
        g_ps2_registered = 1;
        return 0; /* PS/2 logical id is 0 */
    } else {
        /* USB logical ids start at 1, the USB layer keeps details */
        uint8_t id = g_usb_next_id++;
        return id;
    }
}

void kbd_unregister_device(uint8_t logical_id) {
    if (logical_id == 0) {
        g_ps2_registered = 0;
        /* You could also mask IRQ1 if you want */
    } else {
        /* USB devices are handled in USB layer */
    }
}

/* =========================
   PS/2 Set1 scancode handling
   ========================= */
/* Minimal Set1 constants used here */
#define SC_BACKSPACE 0x0E
#define SC_TAB       0x0F
#define SC_ENTER     0x1C
#define SC_ESC       0x01
#define SC_LSHIFT    0x2A
#define SC_RSHIFT    0x36
#define SC_LCTRL     0x1D
#define SC_LALT      0x38
#define SC_CAPSLOCK  0x3A

/* E0-extended mapping (Set1) to extended keycodes */
static keycode_t ps2_e0_to_ext(uint8_t raw) {
    switch (raw) {
        case 0x48: return KC_UP;
        case 0x50: return KC_DOWN;
        case 0x4B: return KC_LEFT;
        case 0x4D: return KC_RIGHT;
        case 0x52: return KC_INSERT;
        case 0x53: return KC_DELETE;
        case 0x47: return KC_HOME;
        case 0x4F: return KC_END;
        case 0x49: return KC_PGUP;
        case 0x51: return KC_PGDN;
        default:   return KC_NONE;
    }
}

char kbd_layout_ascii_from_set1(uint8_t sc, uint16_t mods_state) {
    if (sc >= SC_ASCII_TABLE_LEN) return 0;
    const char *tbl = ((mods_state & KM_CAPS) ^ (mods_state & KM_SHIFT)) ? tbl_shift : tbl_norm;
    char c = tbl[sc];
    return (c == '?') ? 0 : c;
}

static inline char translate_ps2_ascii(uint8_t raw) {
    if (raw == SC_BACKSPACE) return '\b';
    if (raw == SC_TAB)       return '\t';
    if (raw == SC_ENTER)     return '\n';
    if (raw == 0x39)         return ' '; /* space */

    if (raw < 0x40) {
        return kbd_layout_ascii_from_set1(raw, g_mods);
    }
    return 0;
}

static void emit_ps2_event(uint8_t raw, bool make) {
    key_event_t e;
    e.src    = KDEV_SOURCE_PS2;
    e.dev_id = 0;

    /* Map common keys to extended KC_* for PRESS/RELEASE */
    keycode_t code = KC_NONE;
    switch (raw) {
        case SC_LSHIFT:   code = KC_LSHIFT; break;
        case SC_RSHIFT:   code = KC_RSHIFT; break;
        case SC_LCTRL:    code = KC_LCTRL;  break;
        case SC_LALT:     code = KC_LALT;   break;
        case SC_CAPSLOCK: code = KC_CAPS_LOCK; break;
        case SC_BACKSPACE:code = KC_BACKSPACE; break;
        case SC_TAB:      code = KC_TAB; break;
        case SC_ENTER:    code = KC_ENTER; break;
        case SC_ESC:      code = KC_ESC; break;
        default: break;
    }

    if (code != KC_NONE) {
        e.type = make ? KEY_EV_PRESS : KEY_EV_RELEASE;
        e.code = code;
        e.mods = g_mods;
        kbd_dispatch_event(&e);
        return;
    }

    /* Printable ASCII on make only */
    if (make) {
        char ch = translate_ps2_ascii(raw);
        if (ch) {
            e.type = KEY_EV_PRESS;
            e.code = (keycode_t)(uint8_t)ch; /* ASCII code */
            e.mods = g_mods;
            kbd_dispatch_event(&e);
        }
    }
}

/* IRQ1 forwarder (PS/2) */
static void ps2_irq1_handler(registers_t *regs) {
    (void)regs;
    uint8_t sc = port_byte_in(0x60);

    if (sc == 0xE0) { g_ps2_e0 = 1; return; }

    bool break_code = (sc & 0x80) != 0;
    uint8_t raw = (uint8_t)(sc & 0x7F);

    if (g_ps2_e0) {
        g_ps2_e0 = 0;
        keycode_t ext = ps2_e0_to_ext(raw);
        if (ext != KC_NONE) {
            key_event_t e = {
                .type   = break_code ? KEY_EV_RELEASE : KEY_EV_PRESS,
                .code   = ext,
                .mods   = g_mods,
                .src    = KDEV_SOURCE_PS2,
                .dev_id = 0
            };
            kbd_dispatch_event(&e);
            return;
        }
        /* else: unknown E0 code -> fall through */
    }

    /* Maintain modifier bits ASAP so ASCII uses correct state */
    if (raw == SC_LSHIFT) {
        if (break_code) g_mods &= ~KM_SHIFT;
        else            g_mods |=  KM_SHIFT;
    } else if (raw == SC_RSHIFT) {
        if (break_code) g_mods &= ~KM_RSHIFT, g_mods &= ~KM_SHIFT; /* simple aggregate */
        else            g_mods |=  KM_RSHIFT, g_mods |=  KM_SHIFT;
    } else if (raw == SC_LCTRL) {
        if (break_code) g_mods &= ~KM_CTRL;
        else            g_mods |=  KM_CTRL;
    } else if (raw == SC_LALT) {
        if (break_code) g_mods &= ~KM_ALT;
        else            g_mods |=  KM_ALT;
    } else if (raw == SC_CAPSLOCK && !break_code) {
        /* Toggle happens in dispatcher, but we want ASCII to reflect immediately */
        g_mods ^= KM_CAPS;
    }

    emit_ps2_event(raw, !break_code);
}

/* =========================
   Glue for USB module
   ========================= */
void keyboard_internal_push_event_alias(const key_event_t* e) { kbd_dispatch_event(e); }
void keyboard_internal_push_ascii_alias(char c)               { if (c) push_ascii(c); }
