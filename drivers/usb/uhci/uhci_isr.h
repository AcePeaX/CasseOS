#pragma once
#include <stdint.h>
#include "uhci.h"   // usb_controller_t, UHCI_* logs

/* Install ISR for this UHCI controller.
 * Call after: reset, frame list set, interrupts enabled, controller running.
 */
void uhci_install_isr(usb_controller_t* ctrl);

/* Optional bottom-half you may provide elsewhere (keyboard poll/service). */
void uhci_kbd_service(void);
