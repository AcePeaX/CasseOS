#include "usb.h"
#include "drivers/pci.h"
#include "libc/string.h"
#include "cpu/ports.h"


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

        
        if(controller->pci_device->prog_if==0x00){
            printf("The current controller is of type: UHCI\n");
            uhci_initialize_controller(controller);
        } else if(controller->pci_device->prog_if==0x10){
            printf("The current controller is of type: OHCI\n");
        } else if(controller->pci_device->prog_if==0x20){
            printf("The current controller is of type: EHCI\n");
        } else if(controller->pci_device->prog_if==0x30){
            printf("The current controller is of type: xHCI\n");
        }

        printf("Enumerating devices on USB controller %d\n", i);

    }
}

void pci_enable_bus_mastering(pci_device_t *dev) {
    uint16_t command = pci_config_read_word(dev->bus, dev->device, dev->function, 0x04);
    command |= (1 << 2); // Set the Bus Master Enable bit
    pci_config_write_word(dev->bus, dev->device, dev->function, 0x04, command);
}

void uhci_reset_controller(uint16_t io_base) {
    // Set Global Reset bit (bit 2) in the Command Register (offset 0x00)
    port_word_out(io_base + 0x00, 0x0002);
    // Wait for at least 10 microseconds
    for (volatile int i = 0; i < 1000; i++);
    // Clear the Global Reset bit
    port_word_out(io_base + 0x00, 0x0000);
}

uint32_t frame_list[1024] __attribute__((aligned(4096)));

void uhci_set_frame_list_base_address(uint16_t io_base, uint32_t frame_list_phys_addr) {
    // Write to the Frame List Base Address Register (offset 0x08)
    port_dword_out(io_base + 0x08, frame_list_phys_addr);
}

void uhci_initialize_frame_list() {
    for (int i = 0; i < 1024; i++) {
        frame_list[i] = 0x00000001; // Terminate entries
    }
}

void uhci_start_controller(uint16_t io_base) {
    // Set Run/Stop bit (bit 0) in the Command Register
    port_word_out(io_base + 0x00, 0x0001);
}

void uhci_enable_interrupts(uint16_t io_base) {
    // Write to the Interrupt Enable Register (offset 0x04)
    // Enable Host System Error Interrupt, Resume Interrupt, Interrupt on Completion
    port_word_out(io_base + 0x04, 0x0007);
}

void uhci_initialize_controller(usb_controller_t *controller) {
    uint16_t io_base = (uint16_t)controller->base_address;

    pci_enable_bus_mastering(controller->pci_device);

    uhci_reset_controller(io_base);

    uhci_initialize_frame_list();

    // Assuming frame_list is identity-mapped; if not, you need to get the physical address
    uintptr_t frame_list_phys_addr = (uintptr_t) &frame_list;
    uhci_set_frame_list_base_address(io_base, frame_list_phys_addr);

    uhci_start_controller(io_base);

    printf("UHCI Controller initialized at IO base 0x%x\n", io_base);
}

void uhci_reset_port(uint16_t io_base, int port) {
    uint16_t port_base = io_base + 0x10 + (port * 2);
    // Set the Port Reset bit (bit 9)
    port_word_out(port_base, 0x0200);
    // Wait 50 milliseconds
    for (volatile int i = 0; i < 500000; i++);
    // Clear the Port Reset bit
    port_word_out(port_base, 0x0000);
    // Wait for the port to become enabled
    for (volatile int i = 0; i < 100000; i++);
    uint16_t status = port_word_in(port_base);
    if (status & 0x0001) {
        printf("Device connected on port %d\n", port);
    } else {
        printf("No device on port %d\n", port);
    }
}

