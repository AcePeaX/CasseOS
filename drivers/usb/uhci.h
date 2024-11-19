#ifndef USB_UHCI_H
#define USB_UHCI_H

#include <stdint.h>
#include "usb.h"
#include "../screen.h"
#include "usb_descriptors.h"

#define NUM_PORTS 2 // Number of ports the UHCI controller supports
#define PORT_SC_OFFSET 0x10 // Port Status and Control Register offset
#define PORT_RESET 0x0200 // Port Reset bit
#define PORT_ENABLE 0x0004 // Port Enable bit
#define PORT_CONNECT_STATUS 0x0001 // Current Connect Status bit

void uhci_initialize_controller(usb_controller_t *controller);
void uhci_reset_port(uint16_t io_base, int port);

#endif