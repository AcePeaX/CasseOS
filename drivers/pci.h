#ifndef PCI_H
#define PCI_H

#include <stdint.h>

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA 0xCFC

#define MAX_PCI_DEVICES 256

/* PCI device structure */
typedef struct {
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t class_code;
    uint8_t subclass;
    uint8_t prog_if;
    uint8_t bus;
    uint8_t device;
    uint8_t function;
    uint32_t bar[6];        // Base Address Registers (BARs)
    uint8_t is_memory_mapped[6]; // 1 if memory-mapped, 0 if port-mapped
} pci_device_t;

extern pci_device_t pci_devices[MAX_PCI_DEVICES];
extern uint16_t pci_device_count;

uint32_t pci_config_read(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);
uint16_t pci_config_read_word(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);
void pci_config_write_word(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint16_t value);
pci_device_t pci_get_device(uint8_t bus, uint8_t device, uint8_t function);
void pci_read_bars(pci_device_t *dev);
void pci_scan();

#endif