#include "keyboard.h"

// Minimal HID (Usage Page 0x07) to KC mapping
static keycode_t hid_usage_to_kc(uint8_t u)
{
    switch (u) {
        /* control / whitespace */
        case 0x28: return KC_ENTER;
        case 0x2A: return KC_BACKSPACE;
        case 0x2B: return KC_TAB;
        case 0x2C: return KC_SPACE;
        case 0x29: return KC_ESC;

        /* locks */
        case 0x39: return KC_CAPS_LOCK;
        case 0x53: return KC_NUM_LOCK;
        case 0x47: return KC_SCROLL_LOCK;

        /* navigation / edit */
        case 0x49: return KC_INSERT;
        case 0x4A: return KC_HOME;
        case 0x4B: return KC_PGUP;
        case 0x4C: return KC_DELETE;
        case 0x4D: return KC_END;
        case 0x4E: return KC_PGDN;

        /* arrows */
        case 0x4F: return KC_RIGHT;
        case 0x50: return KC_LEFT;
        case 0x51: return KC_DOWN;
        case 0x52: return KC_UP;

        /* function keys */
        case 0x3A: return KC_F1;  case 0x3B: return KC_F2;
        case 0x3C: return KC_F3;  case 0x3D: return KC_F4;
        case 0x3E: return KC_F5;  case 0x3F: return KC_F6;
        case 0x40: return KC_F7;  case 0x41: return KC_F8;
        case 0x42: return KC_F9;  case 0x43: return KC_F10;
        case 0x44: return KC_F11; case 0x45: return KC_F12;

        default:
            /*  For 0x04..0x1D (A..Z) and 0x1E..0x27 (1..0) and punctuation
                you likely want a separate table that considers modifiers
                (Shift/Caps) to yield ASCII. Return KC_NONE here; your
                higher layer can translate printable usages. */
            return KC_NONE;
    }
}

static uint16_t mods_from_hid(uint8_t m)
{
    uint16_t mods = 0;
    if (m & (1<<0)) mods |= KM_CTRL;
    if (m & (1<<1)) mods |= KM_SHIFT;
    if (m & (1<<2)) mods |= KM_ALT;
    if (m & (1<<3)) mods |= KM_GUI;
    if (m & (1<<4)) mods |= KM_RCTRL;
    if (m & (1<<5)) mods |= KM_RSHIFT;
    if (m & (1<<6)) mods |= KM_RALT;
    if (m & (1<<7)) mods |= KM_RGUI;
    return mods;
}

// Call this when your UHCI interrupt IN TD completes with an 8-byte report.
void usbkbd_on_boot_report(uint8_t logical_dev_id, const uint8_t report[8],
                            const uint8_t prev_report[8])
{
    const uint8_t mods     = report[0];
    const uint8_t prevmods = prev_report[0];

    // 1) Emit modifier changes as press/release
    uint16_t now  = mods_from_hid(mods);
    uint16_t prev = mods_from_hid(prevmods);
    uint16_t changed = now ^ prev;

    for (int bit = 0; bit < 8; ++bit) {
        uint16_t mask = 1u << bit;
        if (changed & mask) {
            key_event_t ev = {0};
            ev.src   = KDEV_SOURCE_USB;
            ev.dev_id= logical_dev_id;
            ev.type  = (now & mask) ? KEY_EV_PRESS : KEY_EV_RELEASE;
            ev.code  = (keycode_t[]){KC_LCTRL,KC_LSHIFT,KC_LALT,KC_LGUI,KC_RCTRL,KC_RSHIFT,KC_RALT,KC_RGUI}[bit];
            kbd_dispatch_event(&ev);
        }
    }

    // 2) Diff key arrays (bytes 2..7 are 6-key Rollover)
    // Emit releases for keys no longer present
    for (int i = 2; i < 8; ++i) {
        uint8_t oldu = prev_report[i];
        if (!oldu) continue;
        int still = 0;
        for (int j = 2; j < 8; ++j) if (report[j] == oldu) { still = 1; break; }
        if (!still) {
            key_event_t ev = {0};
            ev.src = KDEV_SOURCE_USB; ev.dev_id = logical_dev_id;
            ev.type = KEY_EV_RELEASE;
            ev.code = hid_usage_to_kc(oldu);
            if (ev.code) kbd_dispatch_event(&ev);
        }
    }
    // Emit presses for new keys now present
    for (int i = 2; i < 8; ++i) {
        uint8_t newu = report[i];
        if (!newu) continue;
        int was = 0;
        for (int j = 2; j < 8; ++j) if (prev_report[j] == newu) { was = 1; break; }
        if (!was) {
            key_event_t ev = {0};
            ev.src = KDEV_SOURCE_USB; ev.dev_id = logical_dev_id;
            ev.type = KEY_EV_PRESS;
            ev.code = hid_usage_to_kc(newu);
            if (ev.code) kbd_dispatch_event(&ev);
        }
    }

    // 3) Key repeat is policy: your input layer can generate KEY_EV_REPEAT from held keys on a timer.
}
