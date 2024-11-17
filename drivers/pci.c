#include "pci.h"
#include "cpu/ports.h" // Your custom I/O functions
#include "screen.h"
#include "libc/string.h"



/* Read a 32-bit value from the PCI configuration space */
uint32_t pci_config_read(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
    uint32_t address = (1 << 31) | // Enable bit
                        (bus << 16) |
                        (device << 11) |
                        (function << 8) |
                        (offset & 0xFC); // Align offset to 4 bytes
    io_dword_out(PCI_CONFIG_ADDRESS, address); // Send address to config address port
    return io_dword_in(PCI_CONFIG_DATA);       // Read value from config data port
}

/* Get vendor and device ID of a PCI device */
pci_device_t pci_get_device(uint8_t bus, uint8_t device, uint8_t function) {
    pci_device_t dev;
    uint32_t data = pci_config_read(bus, device, function, 0x00); // Read vendor/device ID
    dev.vendor_id = data & 0xFFFF;
    dev.device_id = (data >> 16) & 0xFFFF;

    data = pci_config_read(bus, device, function, 0x08); // Read class, subclass, prog IF
    dev.class_code = (data >> 24) & 0xFF;
    dev.subclass = (data >> 16) & 0xFF;
    dev.prog_if = (data >> 8) & 0xFF;

    dev.bus = bus;
    dev.device = device;
    dev.function = function;

    return dev;
}

/* Scan PCI devices for USB controllers */
void pci_scan_for_usb_controllers() {
    for (uint16_t bus_uint16 = 0; bus_uint16 < 256; bus_uint16++) {
        uint8_t bus = bus_uint16;
        for (uint8_t device = 0; device < 32; device++) {
            for (uint8_t function = 0; function < 8; function++) {
                pci_device_t dev = pci_get_device(bus, device, function);
                if (dev.vendor_id != 0xFFFF) { // 0xFFFF indicates no device
                    if (dev.class_code == 0x0C && dev.subclass == 0x03) { // USB controller
                        kprint("Found USB Controller: ");
                        char hex_value_str[16];
                        hex_to_string_trimmed(dev.vendor_id,hex_value_str);
                        kprint(hex_value_str);
                        kprint(" ");
                        hex_to_string_trimmed(dev.device_id,hex_value_str);
                        kprint(hex_value_str);
                        kprint("\n");
                    }
                }
            }
        }
    }
}
