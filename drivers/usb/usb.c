#include "usb.h"
#include "drivers/pci.h"
#include "libc/string.h"
#include "cpu/ports.h"
#include "uhci.h"
#include <stddef.h>


usb_controller_t usb_controllers[16] = {}; // Support up to 16 controllers
uint8_t usb_controller_count = 0;

usb_device_t usb_devices[MAX_USB_DEVICES];
uint8_t usb_device_count = 0;

/* Scan PCI devices for USB controllers */
void pci_scan_for_usb_controllers() {
    for(pci_device_t* dev = pci_devices; dev<pci_devices+pci_device_count; dev++){
        if (dev->vendor_id != 0xFFFF) { // 0xFFFF indicates no device
            if (dev->class_code == 0x0C && dev->subclass == 0x03) { // USB controller
                usb_controllers[usb_controller_count].pci_device = dev;
                usb_controllers[usb_controller_count].base_address = dev->bar[0];
                        
                // Read BARs for this device
                pci_read_bars(dev);


                /*for (int bar_index = 0; bar_index < 6; bar_index++) {
                    if (dev->bar[bar_index] != 0) {
                        kprint("  BAR");
                        char bar_index_str[2];
                        int_to_ascii(bar_index, bar_index_str);
                        kprint(bar_index_str);
                        kprint(": ");
                        printf("%x",dev->bar[bar_index]);
                        if (dev->is_memory_mapped[bar_index]) {
                            kprint(" (MMIO)\n");
                        } else {
                            kprint(" (PMIO)\n");
                        }
                    }
                }*/

                usb_controller_count++;
            }
        }
    }
    return;
}

void usb_enumerate_devices() {
    usb_device_count = 0;

    for (int i = 0; i < usb_controller_count; i++) {
        usb_controller_t *controller = &usb_controllers[i];

        if(controller->pci_device==NULL){
            continue;
        }

        
        if(controller->pci_device->prog_if==0x00){
            printf("The current controller is of type: UHCI\n");
            uhci_initialize_controller(controller);
            uhci_enumerate_devices(controller);
        } else if(controller->pci_device->prog_if==0x10){
            printf("USB driver for OHCI not yet available.\n");
        } else if(controller->pci_device->prog_if==0x20){
            printf("USB driver for EHCI not yet available.\n");
        } else if(controller->pci_device->prog_if==0x30){
            printf("USB driver for xHCI not yet available.\n");
        }
        else{
            printf("USB controller %d not recognized.\n", i);
        }

    }
}

