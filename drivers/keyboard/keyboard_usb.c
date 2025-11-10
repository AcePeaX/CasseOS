#include "keyboard.h"
#include "../../libc/mem.h"
#include <string.h>
#include <stdbool.h>

/* ---- Status flags ----
 * bit0: initialized (slot has valid identity)
 * bit1: enabled/in-use (user or system says active)
 */
#define KB_STAT_INIT    (1u << 0)
#define KB_STAT_ENABLED (1u << 1)

typedef struct {
    uint8_t  status;        /* bit0: init, bit1: enabled */
    uint8_t  address;
    uint8_t  ep;
    uint8_t  interval;
    uint16_t wMaxPacket;
    uint8_t  last_report[8];/* for key release detection */
    uint8_t  last_mods;
} usb_keyboard_device_t;

#ifndef MAX_USB_KEYBOARDS
#define MAX_USB_KEYBOARDS 4
#endif

static usb_keyboard_device_t g_kbds[MAX_USB_KEYBOARDS];

/* Forward decls for pushing into common queues (glue in keyboard_common.c) */
void keyboard_internal_push_event_alias(const KeyEvent* e);
void keyboard_internal_push_ascii_alias(char c);

/* Weak wrappers to avoid making these public */
__attribute__((weak)) void keyboard_internal_push_event(const KeyEvent* e) {
    keyboard_internal_push_event_alias(e);
}
__attribute__((weak)) void keyboard_internal_push_ascii(char c) {
    keyboard_internal_push_ascii_alias(c);
}

/* ------------------------ tiny HID map ------------------------ */
static char hid_usage_to_ascii(uint8_t usage, uint8_t shift_caps)
{
    if (usage >= 0x04 && usage <= 0x1D) {               /* a..z */
        char base = shift_caps ? 'A' : 'a';
        return (char)(base + (usage - 0x04));
    }
    if (usage >= 0x1E && usage <= 0x26) {               /* 1..9 */
        static const char digits[] = "123456789";
        return digits[usage - 0x1E];
    }
    if (usage == 0x27) return '0';                      /* 0 */
    if (usage == 0x2C) return ' ';                      /* Space */
    if (usage == 0x28) return '\n';                     /* Enter */
    if (usage == 0x2A) return '\b';                     /* Backspace */
    return 0;
}

static void hid_mods_unpack(uint8_t mods, int *shift, int *ctrl, int *alt)
{
    *shift = ((mods & (1<<1)) || (mods & (1<<5))) ? 1 : 0;
    *ctrl  = ((mods & (1<<0)) || (mods & (1<<4))) ? 1 : 0;
    *alt   = ((mods & (1<<2)) || (mods & (1<<6))) ? 1 : 0;
}

static int report_contains(const uint8_t rep[8], uint8_t usage)
{
    for (int i = 2; i < 8; ++i) if (rep[i] == usage) return 1;
    return 0;
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
            d->status     = KB_STAT_INIT | KB_STAT_ENABLED; /* init + enabled by default */
            d->address    = address;
            d->ep         = endpoint_addr;
            d->interval   = interval_ms;
            d->wMaxPacket = wMaxPacketSize;
            d->last_mods  = 0;
            memset(d->last_report, 0, sizeof(d->last_report));
            return i;
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
    memset(&g_kbds[dev_index], 0, sizeof(g_kbds[dev_index]));
}

/* Optional helper if you prefer lookups */
static int find_by_addr_ep(uint8_t address, uint8_t ep)
{
    for (int i = 0; i < MAX_USB_KEYBOARDS; ++i) {
        if ((g_kbds[i].status & KB_STAT_INIT) &&
            g_kbds[i].address == address && g_kbds[i].ep == ep) {
            return i;
        }
    }
    return -1;
}

/* Feed one 8-byte report from UHCI */
void keyboard_usb_on_boot_report(int dev_index, const uint8_t report[8])
{
    if (dev_index < 0 || dev_index >= MAX_USB_KEYBOARDS) return;
    usb_keyboard_device_t *dev = &g_kbds[dev_index];
    if ((dev->status & (KB_STAT_INIT | KB_STAT_ENABLED)) != (KB_STAT_INIT | KB_STAT_ENABLED))
        return; /* not initialized or disabled */

    const uint8_t *prev = dev->last_report;
    uint8_t mods_now  = report[0];
    uint8_t mods_prev = prev[0];

    int sh_now, ct_now, al_now, sh_prev, ct_prev, al_prev;
    hid_mods_unpack(mods_now,  &sh_now,  &ct_now,  &al_now);
    hid_mods_unpack(mods_prev, &sh_prev, &ct_prev, &al_prev);

    /* Key releases */
    for (int i = 2; i < 8; ++i) {
        uint8_t u = prev[i];
        if (u && !report_contains(report, u)) {
            KeyEvent e = { .pressed = 0, .ascii = 0, .code = u,
                            .modifiers = (ct_now?1:0) | (sh_now?2:0) | (al_now?4:0) };
            keyboard_internal_push_event(&e);
        }
    }

    /* Key presses */
    for (int i = 2; i < 8; ++i) {
        uint8_t u = report[i];
        if (u && !report_contains(prev, u)) {
            char ch = hid_usage_to_ascii(u, sh_now /* ^ caps if you track LED state */);
            KeyEvent e = { .pressed = 1, .ascii = (uint8_t)ch, .code = u,
                            .modifiers = (ct_now?1:0) | (sh_now?2:0) | (al_now?4:0) };
            keyboard_internal_push_event(&e);
            if (ch) keyboard_internal_push_ascii(ch);
        }
    }

    /* Save last */
    dev->last_mods = mods_now;
    memcpy(dev->last_report, report, 8);
}


/* Weak symbols so we can call into common without creating public API noise */
__attribute__((weak)) void keyboard_internal_push_event(const KeyEvent* e) {
    keyboard_internal_push_event_alias(e);
}
__attribute__((weak)) void keyboard_internal_push_ascii(char c) {
    keyboard_internal_push_ascii_alias(c);
}
