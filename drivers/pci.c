#include "pci.h"
#include "cpu/ports.h" // Your custom I/O functions
#include "screen.h"

pci_device_t pci_devices[MAX_PCI_DEVICES];
uint16_t pci_device_count = 0;

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

uint16_t pci_config_read_word(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
    uint32_t address = (1 << 31) | // Enable bit
                        (bus << 16) |
                        (device << 11) |
                        (function << 8) |
                        (offset & 0xFC); // Align offset to 4 bytes
    io_dword_out(PCI_CONFIG_ADDRESS, address);
    uint32_t data = io_dword_in(PCI_CONFIG_DATA);
    return (uint16_t)((data >> ((offset & 2) * 8)) & 0xFFFF);
}

uint8_t pci_config_read_byte(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
    uint32_t aligned = pci_config_read(bus, device, function, offset & ~3u);
    return (uint8_t)((aligned >> ((offset & 3u) * 8u)) & 0xFFu);
}

void pci_enable_bus_mastering(pci_device_t *dev) {
    uint16_t command = pci_config_read_word(dev->bus, dev->device, dev->function, 0x04);
    command |= (1 << 2); // Set the Bus Master Enable bit
    pci_config_write_word(dev->bus, dev->device, dev->function, 0x04, command);
}

void pci_config_write_word(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint16_t value) {
    uint32_t address = (1 << 31) |
                        (bus << 16) |
                        (device << 11) |
                        (function << 8) |
                        (offset & 0xFC);
    io_dword_out(PCI_CONFIG_ADDRESS, address);
    uint32_t data = io_dword_in(PCI_CONFIG_DATA);

    // Modify the relevant word
    if (offset & 2) {
        data = (data & 0x0000FFFF) | ((uint32_t)value << 16);
    } else {
        data = (data & 0xFFFF0000) | value;
    }

    io_dword_out(PCI_CONFIG_DATA, data);
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


void pci_read_bars(pci_device_t *dev) {
    for (uint8_t bar_index = 0; bar_index < 6; bar_index++) {
        uint8_t off = (uint8_t)(0x10 + (bar_index * 4));
        uint32_t bar_value = pci_config_read(dev->bus, dev->device, dev->function, off);

        if (bar_value == 0) {
            dev->bar[bar_index] = 0;
            dev->is_memory_mapped[bar_index] = 0;
            continue;
        }

        if (bar_value & 0x1) {
            // Port-mapped I/O
            dev->is_memory_mapped[bar_index] = 0;
            dev->bar[bar_index] = bar_value & 0xFFFFFFFC;
        } else {
            // Memory-mapped I/O (lower 32 bits stored)
            dev->is_memory_mapped[bar_index] = 1;
            dev->bar[bar_index] = bar_value & 0xFFFFFFF0;

            // If it's a 64-bit BAR, consume the next slot (we still store only low 32 bits in dev->bar[])
            if ((bar_value & 0x6) == 0x4) {
                (void)pci_config_read(dev->bus, dev->device, dev->function, (uint8_t)(off + 4)); // upper dword
                bar_index++; // Skip next BAR index (part of the 64-bit pair)
            }
        }
    }
}

void pci_scan() {
    pci_device_count = 0;

    for (uint16_t bus_l = 0; bus_l < 256; bus_l++) {
        uint8_t bus = (uint8_t)bus_l;
        for (uint8_t device = 0; device < 32; device++) {
            for (uint8_t function = 0; function < 8; function++) {
                uint32_t id = pci_config_read(bus, device, function, 0x00);
                if (id == 0xFFFFFFFFu) {
                    continue; // No device here
                }

                if (pci_device_count >= MAX_PCI_DEVICES) {
                    kprint("PCI device array full.\n");
                    return;
                }

                pci_device_t *dev = &pci_devices[pci_device_count];

                dev->vendor_id = (uint16_t)(id & 0xFFFFu);
                dev->device_id = (uint16_t)((id >> 16) & 0xFFFFu);

                uint32_t class_data = pci_config_read(bus, device, function, 0x08);
                dev->class_code = (uint8_t)((class_data >> 24) & 0xFFu);
                dev->subclass   = (uint8_t)((class_data >> 16) & 0xFFu);
                dev->prog_if    = (uint8_t)((class_data >> 8)  & 0xFFu);

                dev->bus      = bus;
                dev->device   = device;
                dev->function = function;

                pci_read_bars(dev); // Read BARs (and mark 64-bit pairs)

                // NEW: read legacy INTx wiring
                dev->interrupt_line = pci_config_read_byte(bus, device, function, 0x3C);
                dev->interrupt_pin  = pci_config_read_byte(bus, device, function, 0x3D);

                pci_device_count++;
            }
        }
    }
}