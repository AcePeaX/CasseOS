#include "drivers/block/ahci/ahci.h"
#include "drivers/screen.h"
#include "cpu/timer.h"
#include "kernel/include/kernel/bootinfo.h"
#include "libc/system.h"
#include "libc/mem.h"
#include "libc/string.h"

extern kernel_bootinfo_t kernel_bootinfo;

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

static void log_port_info(uint8_t port_no, volatile ahci_hba_port_t *port) {
    uint32_t ssts = port->ssts;
    uint32_t det = ssts & 0xF;
    uint32_t spd = (ssts >> 4) & 0xF;
    uint32_t ipm = (ssts >> 8) & 0xF;
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
    ctrl->implemented_ports = ctrl->hba_mem->pi;
    ctrl->active_ports = 0;
    debug_puts("[AHCI]   PI=");
    debug_puthex(ctrl->implemented_ports);
    debug_putc('\n');

    if (ctrl->implemented_ports == 0) {
        printf("[AHCI]   no ports implemented\n");
        debug_puts("[AHCI]   no ports implemented\n");
        return;
    }

    for (uint8_t port = 0; port < AHCI_MAX_PORTS; ++port) {
        if ((ctrl->implemented_ports & (1u << port)) == 0) {
            continue;
        }

        volatile ahci_hba_port_t *hba_port = &ctrl->hba_mem->ports[port];
        debug_puts("[AHCI]   port ");
        debug_putc('0' + port);
        debug_puts(" ssts=");
        debug_puthex(hba_port->ssts);
        debug_puts(" sig=");
        debug_puthex(hba_port->sig);
        debug_putc('\n');
        if (!ahci_port_device_present(hba_port)) {
            debug_puts("[AHCI]   port present bit false\n");
            continue;
        }

        ctrl->active_ports |= (1u << port);
        log_port_info(port, hba_port);

        ahci_port_state_t *state = &ctrl->port_state[port];
        memory_set(state, 0, sizeof(*state));
        state->regs = hba_port;
        state->present = true;

        if (!ahci_port_initialize(ctrl, port)) {
            continue;
        }

        ahci_port_identify(ctrl, port);
    }

    if (ctrl->active_ports == 0) {
        printf("[AHCI]   controller has no active ports yet\n");
    }
}

static void add_controller(pci_device_t *dev, uintptr_t abar) {
    if (ahci_controller_count >= AHCI_MAX_CONTROLLERS) {
        printf("[AHCI] controller limit reached, skipping %02x:%02x.%u\n",
               dev->bus, dev->device, dev->function);
        return;
    }

    ahci_controller_t *entry = &ahci_controllers[ahci_controller_count++];
    memory_set(entry, 0, sizeof(*entry));
    entry->pci_dev = dev;
    entry->abar = abar;
    entry->hba_mem = (volatile ahci_hba_mem_t *)(uintptr_t)abar;

    /* Ensure memory space + bus mastering are enabled. */
    uint16_t cmd = pci_config_read_word(dev->bus, dev->device, dev->function, 0x04);
    cmd |= (1u << 1) | (1u << 2); /* MSE | BME */
    pci_config_write_word(dev->bus, dev->device, dev->function, 0x04, cmd);

    /* Claim BIOS -> OS handoff if supported. */
    volatile ahci_hba_mem_t *hba = entry->hba_mem;
    if (hba->cap2 & 0x1) {
        hba->bohc |= (1u << 1);
        for (int i = 0; i < 1000; ++i) {
            if ((hba->bohc & 0x1) == 0) break;
            sleep_ms(1);
        }
    }

    /* Reset HBA and enable AHCI mode. */
    hba->ghc |= 0x1; /* set HR */
    for (int i = 0; i < 100000; ++i) {
        if ((hba->ghc & 0x1) == 0) break;
    }
    hba->ghc |= (1u << 31); /* AE */

    log_ahci_device(dev, abar);
    ahci_probe_controller(entry);
}

void ahci_init(void) {
    debug_puts("[AHCI] init start\n");

    /* Skip AHCI entirely on BIOS boots; only UEFI populates bootinfo. */
    if (kernel_bootinfo.magic != KERNEL_BOOTINFO_MAGIC) {
        debug_puts("[AHCI] BIOS boot detected; skipping AHCI\n");
        ahci_controller_count = 0;
        return;
    }

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
        debug_puts("[AHCI] no controllers\n");
    }
    debug_puts("[AHCI] init done\n");
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
            if (!ctrl->port_state[port].present) {
                continue;
            }
            any = true;
            log_port_info(port, ctrl->port_state[port].regs);
            if (ctrl->port_state[port].identify_valid) {
                printf("[AHCI]     model='%s' sectors=%llu\n",
                       ctrl->port_state[port].model,
                       (unsigned long long)ctrl->port_state[port].sector_count);
            } else {
                printf("[AHCI]     identify data unavailable\n");
            }
        }

        if (!any) {
            printf("[AHCI]   controller has no active ports yet\n");
        }
    }
}
