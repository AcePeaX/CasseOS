// drivers/keyboard/keyboard_usb.c
#include "keyboard.h"
#include "../../libc/mem.h"
#include "libc/string.h"
#include <stdbool.h>

/* ---- Status flags ----
 * bit0: initialized (slot has valid identity)
 * bit1: enabled/in-use
 */
#define KB_STAT_INIT     (1u << 0)
#define KB_STAT_ENABLED  (1u << 1)

typedef struct {
    uint8_t  status;          /* init/enabled flags */
    uint8_t  address;         /* USB address */
    uint8_t  ep;              /* interrupt IN endpoint addr (with dir bit set) */
    uint8_t  interval;        /* polling interval (ms) */
    uint16_t wMaxPacket;      /* usually 8 for boot */
    uint8_t  last_report[8];  /* last 8-byte boot report */
    uint8_t  last_mods;       /* cached modifier byte (report[0]) */
    uint8_t  dev_id;          /* logical id from kbd_register_device() */
} usb_keyboard_device_t;

#ifndef MAX_USB_KEYBOARDS
#define MAX_USB_KEYBOARDS 4
#endif

static usb_keyboard_device_t g_kbds[MAX_USB_KEYBOARDS];

/* ------------------------ helpers ------------------------ */

static inline uint16_t mods_from_hid(uint8_t m)
{
    uint16_t mods = 0;
    if (m & (1<<0)) mods |= KM_CTRL;
    if (m & (1<<1)) mods |= KM_SHIFT;
    if (m & (1<<2)) mods |= KM_ALT;
    if (m & (1<<3)) mods |= KM_GUI;
    if (m & (1<<4)) { mods |= KM_CTRL;  mods |= KM_RCTRL;  }
    if (m & (1<<5)) { mods |= KM_SHIFT; mods |= KM_RSHIFT; }
    if (m & (1<<6)) { mods |= KM_ALT;   mods |= KM_RALT;   }
    if (m & (1<<7)) { mods |= KM_GUI;   mods |= KM_RGUI;   }
    return mods;
}

static inline int report_contains(const uint8_t rep[8], uint8_t usage)
{
    for (int i = 2; i < 8; ++i) if (rep[i] == usage) return 1;
    return 0;
}

typedef struct {
    uint8_t usage;
    uint8_t set1;
} hid_usage_map_t;

static const hid_usage_map_t hid_usage_map[] = {
    {0x04, 0x1E}, {0x05, 0x30}, {0x06, 0x2E}, {0x07, 0x20}, {0x08, 0x12}, {0x09, 0x21},
    {0x0A, 0x22}, {0x0B, 0x23}, {0x0C, 0x17}, {0x0D, 0x24}, {0x0E, 0x25}, {0x0F, 0x26},
    {0x10, 0x32}, {0x11, 0x31}, {0x12, 0x18}, {0x13, 0x19}, {0x14, 0x10}, {0x15, 0x13},
    {0x16, 0x1F}, {0x17, 0x14}, {0x18, 0x16}, {0x19, 0x2F}, {0x1A, 0x11}, {0x1B, 0x2D},
    {0x1C, 0x15}, {0x1D, 0x2C},
    {0x1E, 0x02}, {0x1F, 0x03}, {0x20, 0x04}, {0x21, 0x05}, {0x22, 0x06}, {0x23, 0x07},
    {0x24, 0x08}, {0x25, 0x09}, {0x26, 0x0A}, {0x27, 0x0B},
    {0x2C, 0x39},
    {0x2D, 0x0C}, {0x2E, 0x0D}, {0x2F, 0x1A}, {0x30, 0x1B}, {0x31, 0x2B},
    {0x33, 0x27}, {0x34, 0x28}, {0x35, 0x29},
    {0x36, 0x33}, {0x37, 0x34}, {0x38, 0x35},
};

static uint8_t hid_usage_to_set1(uint8_t usage)
{
    for (unsigned i = 0; i < sizeof(hid_usage_map)/sizeof(hid_usage_map[0]); ++i) {
        if (hid_usage_map[i].usage == usage) return hid_usage_map[i].set1;
    }
    return 0xFF;
}

/* HID Usage (0x07 page) -> extended KC_* (non-printables) */
static keycode_t hid_usage_to_ext_kc(uint8_t u)
{
    switch (u) {
        case 0x28: return KC_ENTER;
        case 0x29: return KC_ESC;
        case 0x2A: return KC_BACKSPACE;
        case 0x2B: return KC_TAB;
        case 0x2C: return KC_SPACE;

        /* Function keys */
        case 0x3A: return KC_F1;  case 0x3B: return KC_F2;  case 0x3C: return KC_F3;
        case 0x3D: return KC_F4;  case 0x3E: return KC_F5;  case 0x3F: return KC_F6;
        case 0x40: return KC_F7;  case 0x41: return KC_F8;  case 0x42: return KC_F9;
        case 0x43: return KC_F10; case 0x44: return KC_F11; case 0x45: return KC_F12;

        /* Locks */
        case 0x39: return KC_CAPS_LOCK;

        /* Navigation (common set; tweak if your device differs) */
        case 0x52: return KC_UP;
        case 0x51: return KC_DOWN;
        case 0x50: return KC_LEFT;
        case 0x4F: return KC_RIGHT;
        case 0x4A: return KC_HOME;
        case 0x4D: return KC_END;
        case 0x4B: return KC_PGUP;
        case 0x4E: return KC_PGDN;
        case 0x49: return KC_INSERT;
        case 0x4C: return KC_DELETE;

        default:   return KC_NONE;
    }
}

/* Printable ASCII from HID usage, given shift/caps state
 * Returns 0 if not a printable ASCII key.
 */
static char hid_usage_to_ascii(uint8_t usage, uint16_t mods_state)
{
    uint8_t sc = hid_usage_to_set1(usage);
    if (sc == 0xFF) return 0;
    return kbd_layout_ascii_from_set1(sc, mods_state);
}

/* Emit a single key event */
static inline void emit_key(bool press, keycode_t kc, uint16_t mods, uint8_t dev_id)
{
    key_event_t ev = {
        .type   = press ? KEY_EV_PRESS : KEY_EV_RELEASE,
        .code   = kc,
        .mods   = mods,
        .src    = KDEV_SOURCE_USB,
        .dev_id = dev_id
    };
    kbd_dispatch_event(&ev);
}

/* Emit modifier key press/release events for changed bits */
static void emit_modifier_deltas(uint16_t prev, uint16_t now, uint8_t dev_id)
{
    uint16_t changed = (prev ^ now);

    struct { uint16_t mask; keycode_t kc; } map[] = {
        { KM_CTRL,   KC_LCTRL }, { KM_SHIFT,  KC_LSHIFT }, { KM_ALT,   KC_LALT }, { KM_GUI,   KC_LGUI },
        { KM_RCTRL,  KC_RCTRL }, { KM_RSHIFT, KC_RSHIFT }, { KM_RALT,  KC_RALT }, { KM_RGUI,  KC_RGUI },
    };

    for (unsigned i = 0; i < sizeof(map)/sizeof(map[0]); ++i) {
        if (changed & map[i].mask) {
            bool pressed = (now & map[i].mask) != 0;
            emit_key(pressed, map[i].kc, now, dev_id);
        }
    }
}

/* ----------------------- Public USB API ----------------------- */

int keyboard_register_usb_boot_keyboard(uint8_t address,
                                        uint8_t endpoint_addr,
                                        uint8_t interval_ms,
                                        uint16_t wMaxPacketSize)
{
    for (int i = 0; i < MAX_USB_KEYBOARDS; ++i) {
        if (!(g_kbds[i].status & KB_STAT_INIT)) {
            usb_keyboard_device_t *d = &g_kbds[i];

            d->status     = KB_STAT_INIT | KB_STAT_ENABLED;
            d->address    = address;
            d->ep         = endpoint_addr;
            d->interval   = interval_ms;
            d->wMaxPacket = wMaxPacketSize;
            d->last_mods  = 0;
            memset(d->last_report, 0, sizeof(d->last_report));

            /* Get a logical device id in the unified keyboard layer */
            d->dev_id = kbd_register_device(KDEV_SOURCE_USB, address);

            return i; /* return our local slot index */
        }
    }
    return -1;
}

void keyboard_usb_set_enabled(int dev_index, bool enabled)
{
    if (dev_index < 0 || dev_index >= MAX_USB_KEYBOARDS) return;
    usb_keyboard_device_t *d = &g_kbds[dev_index];
    if (!(d->status & KB_STAT_INIT)) return;
    if (enabled) d->status |= KB_STAT_ENABLED;
    else         d->status &= ~KB_STAT_ENABLED;
}

bool keyboard_usb_is_enabled(int dev_index)
{
    if (dev_index < 0 || dev_index >= MAX_USB_KEYBOARDS) return false;
    const usb_keyboard_device_t *d = &g_kbds[dev_index];
    return (d->status & (KB_STAT_INIT | KB_STAT_ENABLED)) == (KB_STAT_INIT | KB_STAT_ENABLED);
}

void keyboard_usb_unregister(int dev_index)
{
    if (dev_index < 0 || dev_index >= MAX_USB_KEYBOARDS) return;
    usb_keyboard_device_t *d = &g_kbds[dev_index];
    if (d->status & KB_STAT_INIT) {
        kbd_unregister_device(d->dev_id);
    }
    memset(d, 0, sizeof(*d));
}

/* Optional lookup by (address,ep) if you prefer that route */
int keyboard_usb_find_by_addr_ep(uint8_t address, uint8_t ep)
{
    for (int i = 0; i < MAX_USB_KEYBOARDS; ++i) {
        if ((g_kbds[i].status & KB_STAT_INIT) &&
            g_kbds[i].address == address && g_kbds[i].ep == ep) {
            return i;
        }
    }
    return -1;
}

/* Feed one 8-byte HID Boot Keyboard report from your UHCI ISR/poller */
void keyboard_usb_on_boot_report(int dev_index, const uint8_t report[8])
{
    if (dev_index < 0 || dev_index >= MAX_USB_KEYBOARDS) return;
    usb_keyboard_device_t *dev = &g_kbds[dev_index];
    if ((dev->status & (KB_STAT_INIT | KB_STAT_ENABLED)) != (KB_STAT_INIT | KB_STAT_ENABLED))
        return;

    const uint8_t *prev = dev->last_report;

    uint8_t mods_now  = report[0];
    uint8_t mods_prev = prev[0];

    uint16_t m_now  = mods_from_hid(mods_now);
    uint16_t m_prev = mods_from_hid(mods_prev);

    /* 1) Emit modifier key transitions */
    if (m_now != m_prev) {
        emit_modifier_deltas(m_prev, m_now, dev->dev_id);
    }

    /* 2) Emit releases for keys no longer present */
    for (int i = 2; i < 8; ++i) {
        uint8_t u = prev[i];
        if (u && !report_contains(report, u)) {
            keycode_t kc = hid_usage_to_ext_kc(u);
            if (kc == KC_NONE) {
                /* If it was printable ASCII before, compute as ASCII (case doesn't matter on release) */
                char ch = hid_usage_to_ascii(u, 0);
                if (ch) kc = (keycode_t)(uint8_t)ch;
            }
            if (kc != KC_NONE)
                emit_key(false, kc, m_now, dev->dev_id);
        }
    }

    /* 3) Emit presses for newly present keys */
    for (int i = 2; i < 8; ++i) {
        uint8_t u = report[i];
        if (u && !report_contains(prev, u)) {
            /* Prefer printable ASCII when possible */
            uint16_t mods_snapshot = kbd_mods_state();
            char ch = hid_usage_to_ascii(u, mods_snapshot);
            keycode_t kc = KC_NONE;

            if (ch) {
                kc = (keycode_t)(uint8_t)ch;
            } else {
                kc = hid_usage_to_ext_kc(u);
            }

            if (kc != KC_NONE)
                emit_key(true, kc, m_now, dev->dev_id);
        }
    }

    /* Save last */
    dev->last_mods = mods_now;
    memcpy(dev->last_report, report, 8);
}
