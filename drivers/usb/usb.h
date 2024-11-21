#ifndef USB_CORE_H
#define USB_CORE_H

#include <stdint.h>
#include "../pci.h"
#include "../screen.h"
#include "usb_descriptors.h"

typedef struct {
    pci_device_t *pci_device;
    uint32_t base_address; // Base address for memory-mapped or port-mapped I/O
} usb_controller_t;

extern usb_controller_t usb_controllers[16]; // Support up to 16 controllers
extern uint8_t usb_controller_count;

void pci_scan_for_usb_controllers();
void usb_enumerate_devices();
//void usb_init();

#endif