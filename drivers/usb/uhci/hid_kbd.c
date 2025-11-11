// drivers/usb/uhci/hid_kbd.c
#include "uhci.h"
#include "../../keyboard/keyboard.h"
#include "libc/mem.h"
#include "cpu/ports.h"

#define TD_ACTIVE   (1u << 23)   // 0x0080_0000 in your code
#define TD_IOC      (1u << 24)   // raise IRQ on completion
#define TD_SPD      (1u << 29)   // Short Packet Detect (recommended for IN)

#define TD_STALLED  (1u << 22)
#define TD_DBE      (1u << 21)
#define TD_BABBLE   (1u << 20)
#define TD_NAK      (1u << 19)
#define TD_TIMEOUT  (1u << 18)
#define TD_BITSTUFF (1u << 17)
#define TD_ERR_MASK (TD_STALLED | TD_DBE | TD_BABBLE | TD_TIMEOUT | TD_BITSTUFF)
#define TD_ACTLEN_MASK 0x7FF

typedef struct {
    uint8_t      in_use;
    uint16_t     io_base;
    uint8_t      dev_addr;
    uint8_t      ep;            // endpoint number (0..15)
    uint8_t      interval;      // in frames (ms at FS)
    uint8_t      dev_index;     // from keyboard_register_usb_boot_keyboard(...)
    uint8_t      toggle;        // DATA0/1 -> bit19 in token
    uint16_t     first_slot;    // first frame slot used
    uint8_t      *buf;          // 8-byte report
    uhci_td_t    *td;           // single persistent TD
    uhci_qh_t    *qh;           // QH anchoring the TD
} uhci_kbd_pipe_t;

#ifndef UHCI_MAX_KBD_PIPES
#define UHCI_MAX_KBD_PIPES 4
#endif

static uhci_kbd_pipe_t g_kbd_pipes[UHCI_MAX_KBD_PIPES];

static inline uint8_t ep_number_from_addr(uint8_t endpoint_address) {
    // USB endpoint address packs dir in bit7, number in bits 0..3
    return (endpoint_address & 0x0F);
}

static inline uint32_t uhci_build_in_token(uint8_t addr, uint8_t ep, uint8_t toggle, uint16_t max_len)
{
    // UHCI token layout you’re already using:
    // PID_IN = 0x69
    // bits 8..14 = device address
    // bits 15..18 = endpoint
    // bit  19     = data toggle
    // bits 21..31 = MaxLen-1
    uint32_t m1 = (max_len ? (max_len - 1) : 0) & 0x7FF;
    return  0x69
            | ((uint32_t)addr << 8)
            | ((uint32_t)(ep & 0x0F) << 15)
            | ((uint32_t)(toggle ? 1 : 0) << 19)
            | (m1 << 21);
}

static void place_qh_every_interval(uhci_kbd_pipe_t *p)
{
    // Place the QH into the frame list every 'interval' frames.
    // Use current frame number + 1 as a starting slot to avoid racing the current frame.
    uint16_t io = p->io_base;
    uint16_t start = ((uint16_t)port_word_in(io + 0x06) & 0x7FF) + 1;
    p->first_slot = start % 1024;

    for (uint16_t f = p->first_slot; f < 1024; f = (uint16_t)((f + p->interval) % 1024)) {
        // Protect against interval==0 (shouldn’t happen for HID; spec says >=1). Just in case:
        if (p->interval == 0) break;
        frame_list[f] = get_physical_address(p->qh) | 0x00000002; // QH bit set
        // Stop once we wrap and hit the first slot again.
        if ((uint16_t)((f + p->interval) % 1024) == p->first_slot) break;
    }
}

static void uhci_kbd_rearm_td(uhci_kbd_pipe_t *p, bool advance_toggle)
{
    if (advance_toggle) {
        p->toggle ^= 1;
    }

    p->td->token = uhci_build_in_token(p->dev_addr, p->ep, p->toggle, 8);
    p->td->control_status  = TD_ACTLEN_MASK;
    p->td->control_status |= TD_SPD | TD_IOC | TD_ACTIVE;
    p->qh->vertical_link_pointer = get_physical_address(p->td);

    __asm__ __volatile__("" ::: "memory");
}

int uhci_kbd_open_interrupt_in(uint16_t io_base,
                                uint8_t dev_addr,
                                uint8_t endpoint_address,
                                uint8_t interval_frames,
                                uint16_t wMaxPacket,
                                int keyboard_dev_index)
{
    for (int i = 0; i < UHCI_MAX_KBD_PIPES; ++i) {
        if (!g_kbd_pipes[i].in_use) {
            uhci_kbd_pipe_t *p = &g_kbd_pipes[i];
            memory_set(p, 0, sizeof(*p));
            p->in_use   = 1;
            p->io_base  = io_base;
            p->dev_addr = dev_addr;
            p->ep       = ep_number_from_addr(endpoint_address);
            p->interval = (interval_frames == 0) ? 1 : interval_frames;
            p->dev_index= (uint8_t)keyboard_dev_index;
            p->toggle   = 0; // HID interrupt IN typically starts with DATA1 (many stacks do this)

            // Allocate objects
            p->buf = (uint8_t*)aligned_alloc(16, 8);
            if (!p->buf) goto fail;
            memory_set(p->buf, 0, 8);

            p->td = (uhci_td_t*)aligned_alloc(16, sizeof(uhci_td_t));
            p->qh = (uhci_qh_t*)aligned_alloc(16, sizeof(uhci_qh_t));
            if (!p->td || !p->qh) goto fail;

            memory_set(p->td, 0, sizeof(*p->td));
            memory_set(p->qh, 0, sizeof(*p->qh));

            // TD
            p->td->link_pointer   = 0x00000001; // terminate
            p->td->buffer_pointer = get_physical_address(p->buf);
            uhci_kbd_rearm_td(p, false);

            // QH
            p->qh->horizontal_link_pointer = 0x00000001; // terminate
            p->qh->vertical_link_pointer   = get_physical_address(p->td);

            // Schedule QH in frame list every interval frames
            place_qh_every_interval(p);

            return i;

        fail:
            if (p->td) aligned_free(p->td);
            if (p->qh) aligned_free(p->qh);
            if (p->buf) aligned_free(p->buf);
            memory_set(p, 0, sizeof(*p));
            return -1;
        }
    }
    return -1;
}

void uhci_kbd_close(int pipe_id)
{
    if (pipe_id < 0 || pipe_id >= UHCI_MAX_KBD_PIPES) return;
    uhci_kbd_pipe_t *p = &g_kbd_pipes[pipe_id];
    if (!p->in_use) return;

    // Remove QH from the frame list slots we used
    if (p->interval) {
        for (uint16_t f = p->first_slot; f < 1024; f = (uint16_t)((f + p->interval) % 1024)) {
            frame_list[f] = 0x00000001; // terminate
            if ((uint16_t)((f + p->interval) % 1024) == p->first_slot) break;
        }
    }

    if (p->td)  aligned_free(p->td);
    if (p->qh)  aligned_free(p->qh);
    if (p->buf) aligned_free(p->buf);
    memory_set(p, 0, sizeof(*p));
}

/* Call this from your UHCI ISR/poller after you notice IOC or periodically.
 * It checks all keyboard pipes; if a TD completed, it hands the 8-byte report
 * to the keyboard layer and re-arms the TD (toggle DATA, set Active).
 */
void uhci_kbd_service(void)
{
    for (int i = 0; i < UHCI_MAX_KBD_PIPES; ++i) {
        uhci_kbd_pipe_t *p = &g_kbd_pipes[i];
        if (!p->in_use) continue;

        uhci_td_t *td = p->td;
        uint32_t cs = td->control_status;

        // Active bit is bit 23 (0x800000) in your code. If Active is still set, not done yet.
        if (cs & TD_ACTIVE) continue;  // bit23 ACTIVE

        // NAK (no new data): just re-arm without toggling so DATA sync stays valid.
        if (cs & TD_NAK) {
            uhci_kbd_rearm_td(p, false);
            continue;
        }

        // Any other error (stall, babble, etc.): warn once and re-arm without consuming data.
        if (cs & TD_ERR_MASK) {
            UHCI_WARN("UHCI KBD TD error (cs=0x%x)\n", cs);
            uhci_kbd_rearm_td(p, false);
            continue;
        }

        // We have a fresh 8-byte report in p->buf
        keyboard_usb_on_boot_report(p->dev_index, p->buf);

        // Re-arm for next poll, only toggling when we consumed real data.
        uhci_kbd_rearm_td(p, true);
    }
}
