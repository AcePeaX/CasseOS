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

#define UHCI_TD_STATUS_ACTIVE (1 << 23)


typedef struct {
    uint32_t link_pointer;
    uint32_t control_status;
    uint32_t token;
    uint32_t buffer_pointer;
} __attribute__((packed, aligned(16))) uhci_td_t;

typedef struct {
    uint32_t horizontal_link_pointer;
    uint32_t vertical_link_pointer;
} __attribute__((packed, aligned(16))) uhci_qh_t;

uint32_t find_uhci_io_base(pci_device_t* device);
bool uhci_initialize_controller(usb_controller_t *controller);
void uhci_reset_port(uint16_t io_base, int port);
void uhci_enumerate_device(uint16_t io_base, int port);
void uhci_enumerate_devices(usb_controller_t* controller);

#endif