#include "drivers/block/ahci.h"
#include "drivers/screen.h"

#define PCI_CLASS_MASS_STORAGE 0x01
#define PCI_SUBCLASS_SATA      0x06
#define PCI_PROG_IF_AHCI       0x01
#define AHCI_BAR_INDEX         5

#define AHCI_SSTS_DET_MASK 0xF
#define AHCI_SSTS_SPD_MASK 0xF0
#define AHCI_SSTS_IPM_MASK 0xF00
#define AHCI_DET_PRESENT   0x3

ahci_controller_t ahci_controllers[AHCI_MAX_CONTROLLERS];
uint32_t ahci_controller_count = 0;

static const char *ahci_signature_to_string(uint32_t sig) {
    switch (sig) {
        case AHCI_SIG_SATA:   return "SATA";
        case AHCI_SIG_SATAPI: return "SATAPI";
        case AHCI_SIG_SEMB:   return "SEMB";
        case AHCI_SIG_PM:     return "PORT-MULT";
        default:              return "UNKNOWN";
    }
}

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

static bool ahci_port_device_present(volatile ahci_hba_port_t *port) {
    uint32_t ssts = port->ssts;
    uint32_t det = ssts & AHCI_SSTS_DET_MASK;
    uint32_t spd = (ssts & AHCI_SSTS_SPD_MASK) >> 4;
    if (det != AHCI_DET_PRESENT) {
        return false;
    }

    if (spd == 0) {
        return false;
    }

    return true;
}

static void log_port_info(uint8_t port_no, volatile ahci_hba_port_t *port) {
    uint32_t ssts = port->ssts;
    uint32_t det = ssts & AHCI_SSTS_DET_MASK;
    uint32_t spd = (ssts & AHCI_SSTS_SPD_MASK) >> 4;
    uint32_t ipm = (ssts & AHCI_SSTS_IPM_MASK) >> 8;
    uint32_t sig = port->sig;

    printf("[AHCI]   port %02u type=%s sig=0x%08x det=%u spd=%u ipm=%u\n",
           port_no,
           ahci_signature_to_string(sig),
           sig,
           det,
           spd,
           ipm);
}

static void ahci_probe_controller(ahci_controller_t *ctrl) {
    ctrl->hba_mem = (volatile ahci_hba_mem_t *)(uintptr_t)ctrl->abar;
    ctrl->implemented_ports = ctrl->hba_mem->pi;
    ctrl->active_ports = 0;

    if (ctrl->implemented_ports == 0) {
        printf("[AHCI]   no ports implemented\n");
        return;
    }

    for (uint8_t port = 0; port < AHCI_MAX_PORTS; ++port) {
        if ((ctrl->implemented_ports & (1u << port)) == 0) {
            continue;
        }

        volatile ahci_hba_port_t *hba_port = &ctrl->hba_mem->ports[port];
        if (!ahci_port_device_present(hba_port)) {
            continue;
        }

        ctrl->active_ports |= (1u << port);
        log_port_info(port, hba_port);
    }

    if (ctrl->active_ports == 0) {
        printf("[AHCI]   controller has no active ports yet\n");
    }
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
    entry->hba_mem = NULL;
    entry->implemented_ports = 0;
    entry->active_ports = 0;

    log_ahci_device(dev, abar);
    ahci_probe_controller(entry);
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

void ahci_print_summary(void) {
    if (ahci_controller_count == 0) {
        printf("[AHCI] No controllers registered.\n");
        return;
    }

    for (uint32_t i = 0; i < ahci_controller_count; ++i) {
        ahci_controller_t *ctrl = &ahci_controllers[i];
        log_ahci_device(ctrl->pci_dev, ctrl->abar);

        if (ctrl->implemented_ports == 0) {
            printf("[AHCI]   no ports implemented\n");
            continue;
        }

        bool any = false;
        for (uint8_t port = 0; port < AHCI_MAX_PORTS; ++port) {
            if ((ctrl->implemented_ports & (1u << port)) == 0) {
                continue;
            }

            volatile ahci_hba_port_t *hba_port = &ctrl->hba_mem->ports[port];
            if (!ahci_port_device_present(hba_port)) {
                continue;
            }

            any = true;
            log_port_info(port, hba_port);
        }

        if (!any) {
            printf("[AHCI]   controller has no active ports yet\n");
        }
    }
}
