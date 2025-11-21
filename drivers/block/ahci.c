#include "drivers/block/ahci.h"
#include "drivers/screen.h"

#define PCI_CLASS_MASS_STORAGE 0x01
#define PCI_SUBCLASS_SATA      0x06
#define PCI_PROG_IF_AHCI       0x01
#define AHCI_BAR_INDEX         5

ahci_controller_t ahci_controllers[AHCI_MAX_CONTROLLERS];
uint32_t ahci_controller_count = 0;

static void log_ahci_device(const pci_device_t *dev, uintptr_t abar) {
    printf("[AHCI] %02x:%02x.%u vendor=%04x device=%04x abar=0x%x irq=%u pin=%u\n",
           dev->bus,
           dev->device,
           dev->function,
           dev->vendor_id,
           dev->device_id,
           (unsigned int)abar,
           dev->interrupt_line,
           dev->interrupt_pin);
}

static void add_controller(pci_device_t *dev, uintptr_t abar) {
    if (ahci_controller_count >= AHCI_MAX_CONTROLLERS) {
        printf("[AHCI] Controller limit reached, skipping %02x:%02x.%u\n",
               dev->bus, dev->device, dev->function);
        return;
    }

    ahci_controller_t *entry = &ahci_controllers[ahci_controller_count++];
    entry->pci_dev = dev;
    entry->abar = abar;

    log_ahci_device(dev, abar);
}

void ahci_init(void) {
    ahci_controller_count = 0;

    for (uint16_t i = 0; i < pci_device_count; ++i) {
        pci_device_t *dev = &pci_devices[i];
        if (dev->class_code != PCI_CLASS_MASS_STORAGE ||
            dev->subclass != PCI_SUBCLASS_SATA ||
            dev->prog_if != PCI_PROG_IF_AHCI) {
            continue;
        }

        if (!dev->is_memory_mapped[AHCI_BAR_INDEX]) {
            printf("[AHCI] %02x:%02x.%u missing MMIO BAR%u\n",
                   dev->bus, dev->device, dev->function, AHCI_BAR_INDEX);
            continue;
        }

        uintptr_t abar = dev->bar[AHCI_BAR_INDEX];
        if (abar == 0) {
            printf("[AHCI] %02x:%02x.%u BAR%u is zero\n",
                   dev->bus, dev->device, dev->function, AHCI_BAR_INDEX);
            continue;
        }

        add_controller(dev, abar);
    }

    if (ahci_controller_count == 0) {
        printf("[AHCI] No AHCI controllers detected.\n");
    }
}
