#include "uhci_isr.h"
#include "cpu/isr.h"
#include "cpu/ports.h"

#define USBSTS_USBINT     (1u << 0)  // Interrupt on Completion
#define USBSTS_USBERRINT  (1u << 1)  // USB Error
#define USBSTS_RESUMEDET  (1u << 2)  // Resume Detect
#define USBSTS_HCERR      (1u << 3)  // Host Controller Process Error
#define USBSTS_HCHALTED   (1u << 5)  // Controller Halted

typedef struct {
    uint16_t io_base;
    uint8_t  irq_line;  // 0..15 (legacy INTx)
} uhci_isr_ctx_t;

static uhci_isr_ctx_t g_uhci_ctx;

/* Map legacy IRQ line to your vector constants in cpu/isr.h */
static inline uint8_t irq_to_vector(uint8_t irq_line) { return (uint8_t)(IRQ0 + irq_line); }

/* --- Top-half ISR (keep it very fast) --- */
static void uhci_irq_top(registers_t* r)
{
    printf("IRQ Execute!\n");
    (void)r;
    const uint16_t io = g_uhci_ctx.io_base;

    /* Read UHCI status (write-1-to-clear) */
    uint16_t st = port_word_in(io + 0x02);
    if (!st) {
        /* Shared/spurious IRQ: nothing for this controller */
        return;
    }

    /* Ack the causes we saw */
    port_word_out(io + 0x02, st);

    if (st & USBSTS_USBINT) {
        /* TD(s) completed: service periodic endpoints (e.g., boot keyboard) */
        if (uhci_kbd_service) uhci_kbd_service();
    }
    if (st & USBSTS_USBERRINT) {
        UHCI_WARN("UHCI: USBERRINT (USBSTS=0x%x)\n", st);
        if (uhci_kbd_service) uhci_kbd_service(); /* still try to drain/rearm */
    }
    if (st & USBSTS_RESUMEDET) {
        UHCI_INFO("UHCI: Resume detected\n");
    }
    if (st & USBSTS_HCERR) {
        UHCI_ERR("UHCI: Host Controller Process Error (USBSTS=0x%x)\n", st);
    }
    if (st & USBSTS_HCHALTED) {
        /* Usually means the HC stopped. If persistent, consider reset/restart. */
        UHCI_WARN("UHCI: HCHalted observed\n");
    }

    /* NOTE: Do NOT send PIC EOI here — your generic IRQ path should do that,
       exactly like it does for the PS/2 keyboard handler. */
}

/* --- Public: install the ISR on this controller's legacy IRQ line --- */
void uhci_install_isr(usb_controller_t* ctrl)
{
    g_uhci_ctx.io_base  = (uint16_t)ctrl->base_address;
    g_uhci_ctx.irq_line = ctrl->pci_device->interrupt_line;  // 0..15 expected

    if (g_uhci_ctx.irq_line >= 16) {
        UHCI_ERR("UHCI: invalid PCI interrupt_line=%u\n", (unsigned)g_uhci_ctx.irq_line);
        return;
    }

    /* Register the handler on that vector */
    register_interrupt_handler(irq_to_vector(g_uhci_ctx.irq_line), uhci_irq_top);

    UHCI_INFO("UHCI: ISR installed on IRQ%u (vector=%u), IO base=0x%x\n",
                (unsigned)g_uhci_ctx.irq_line,
                (unsigned)irq_to_vector(g_uhci_ctx.irq_line),
                (unsigned)g_uhci_ctx.io_base);
}
