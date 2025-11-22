#include "drivers/block/ahci.h"
#include "drivers/screen.h"
#include "libc/mem.h"
#include "libc/string.h"

#define PCI_CLASS_MASS_STORAGE 0x01
#define PCI_SUBCLASS_SATA      0x06
#define PCI_PROG_IF_AHCI       0x01
#define AHCI_BAR_INDEX         5

#define AHCI_SSTS_DET_MASK 0xF
#define AHCI_SSTS_SPD_MASK 0xF0
#define AHCI_SSTS_IPM_MASK 0xF00
#define AHCI_DET_PRESENT   0x3

#define AHCI_GENERIC_TIMEOUT 1000000

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

static void ahci_decode_ident_string(char *dst, size_t dst_len, const uint16_t *src, size_t word_count) {
    size_t chars = word_count * 2;
    if (chars >= dst_len) {
        chars = dst_len - 1;
    }

    for (size_t i = 0; i < chars / 2; ++i) {
        uint16_t word = src[i];
        dst[i * 2] = (char)(word >> 8);
        if ((i * 2 + 1) < dst_len - 1) {
            dst[i * 2 + 1] = (char)(word & 0xFF);
        }
    }
    dst[chars] = '\0';

    // Trim trailing spaces
    for (int i = (int)chars - 1; i >= 0; --i) {
        if (dst[i] == ' ' || dst[i] == '\0') {
            dst[i] = '\0';
        } else {
            break;
        }
    }
}

static void ahci_port_stop(volatile ahci_hba_port_t *port) {
    uint32_t cmd = port->cmd;
    if (cmd & HBA_PxCMD_ST) {
        port->cmd = cmd & ~HBA_PxCMD_ST;
        do {
            cmd = port->cmd;
        } while (cmd & HBA_PxCMD_CR);
    }

    cmd = port->cmd;
    if (cmd & HBA_PxCMD_FRE) {
        port->cmd = cmd & ~HBA_PxCMD_FRE;
        do {
            cmd = port->cmd;
        } while (cmd & HBA_PxCMD_FR);
    }
}

static void ahci_port_start(volatile ahci_hba_port_t *port) {
    while (port->cmd & (HBA_PxCMD_CR | HBA_PxCMD_FR)) {
        // wait for controller to acknowledge previous stop
    }
    port->cmd |= HBA_PxCMD_FRE;
    port->cmd |= HBA_PxCMD_ST;
}

static bool ahci_wait_ready(volatile ahci_hba_port_t *port) {
    for (uint32_t i = 0; i < AHCI_GENERIC_TIMEOUT; ++i) {
        uint32_t tfd = port->tfd;
        if (((tfd & HBA_PxTFD_BSY) == 0) && ((tfd & HBA_PxTFD_DRQ) == 0)) {
            return true;
        }
    }
    printf("[AHCI]   timeout waiting for port ready\n");
    return false;
}

static bool ahci_wait_for_command(volatile ahci_hba_port_t *port, uint32_t slot) {
    uint32_t mask = (1u << slot);
    for (uint32_t i = 0; i < AHCI_GENERIC_TIMEOUT; ++i) {
        if ((port->ci & mask) == 0) {
            return true;
        }
        if (port->is & HBA_PxIS_TFES) {
            printf("[AHCI]   task file error during command\n");
            return false;
        }
    }
    printf("[AHCI]   timeout waiting for command completion\n");
    return false;
}

static bool ahci_port_identify(ahci_controller_t *ctrl, uint8_t port_no) {
    ahci_port_state_t *state = &ctrl->port_state[port_no];
    volatile ahci_hba_port_t *port = state->regs;
    if (!state->initialized || !state->cmd_headers || !state->cmd_table) {
        return false;
    }

    if (!ahci_wait_ready(port)) {
        return false;
    }

    const uint32_t slot = 0;
    ahci_command_header_t *cmd_header = &state->cmd_headers[slot];
    memory_set(cmd_header, 0, sizeof(*cmd_header));
    cmd_header->dw0 = (sizeof(fis_reg_h2d_t) / 4) & 0x1F;
    cmd_header->dw0 |= (1u << 16); // one PRDT entry
    cmd_header->ctba = (uint32_t)(uintptr_t)state->cmd_table;
    cmd_header->ctbau = 0;
    cmd_header->prdbc = 0;

    ahci_command_table_t *cmd_table = state->cmd_table;
    memory_set(cmd_table, 0, sizeof(*cmd_table));

    uint16_t identify_buffer[256];
    memory_set(identify_buffer, 0, sizeof(identify_buffer));

    ahci_prdt_entry_t *prdt = &cmd_table->prdt_entry[0];
    prdt->dba = (uint32_t)(uintptr_t)identify_buffer;
    prdt->dbau = 0;
    prdt->reserved0 = 0;
    prdt->dbc = (512 - 1) | (1u << 31); // byte count and interrupt

    fis_reg_h2d_t *cmd_fis = (fis_reg_h2d_t *)cmd_table->cfis;
    memory_set(cmd_fis, 0, sizeof(*cmd_fis));
    cmd_fis->fis_type = FIS_TYPE_REG_H2D;
    cmd_fis->pm_and_c = (1u << 7); // command
    cmd_fis->command = 0xEC; // IDENTIFY DEVICE
    cmd_fis->device = (1u << 6); // LBA mode

    port->is = (uint32_t)-1;
    port->ci = (1u << slot);

    if (!ahci_wait_for_command(port, slot)) {
        return false;
    }

    ahci_decode_ident_string(state->model, sizeof(state->model), &identify_buffer[27], 20);

    uint32_t sector_low = identify_buffer[60] | ((uint32_t)identify_buffer[61] << 16);
    uint64_t sector_high =
        ((uint64_t)identify_buffer[103] << 48) |
        ((uint64_t)identify_buffer[102] << 32) |
        ((uint64_t)identify_buffer[101] << 16) |
        ((uint64_t)identify_buffer[100]);

    if (sector_high != 0) {
        state->sector_count = sector_high;
    } else {
        state->sector_count = sector_low;
    }

    state->identify_valid = true;
    printf("[AHCI]   port %02u drive model='%s' sectors=%llu\n",
           port_no,
           state->model,
           (unsigned long long)state->sector_count);
    return true;
}

static bool ahci_port_initialize(ahci_controller_t *ctrl, uint8_t port_no) {
    ahci_port_state_t *state = &ctrl->port_state[port_no];
    volatile ahci_hba_port_t *port = state->regs;

    ahci_port_stop(port);

    size_t cmd_list_bytes = AHCI_MAX_COMMAND_SLOTS * sizeof(ahci_command_header_t);
    state->cmd_headers = (ahci_command_header_t *)aligned_alloc(1024, cmd_list_bytes);
    if (!state->cmd_headers) {
        printf("[AHCI]   failed to allocate command list for port %u\n", port_no);
        return false;
    }
    memory_set(state->cmd_headers, 0, cmd_list_bytes);

    state->recv_fis = (uint8_t *)aligned_alloc(256, 256);
    if (!state->recv_fis) {
        printf("[AHCI]   failed to allocate FIS buffer for port %u\n", port_no);
        return false;
    }
    memory_set(state->recv_fis, 0, 256);

    state->cmd_table = (ahci_command_table_t *)aligned_alloc(128, sizeof(ahci_command_table_t));
    if (!state->cmd_table) {
        printf("[AHCI]   failed to allocate command table for port %u\n", port_no);
        return false;
    }
    memory_set(state->cmd_table, 0, sizeof(ahci_command_table_t));

    port->clb = (uint32_t)(uintptr_t)state->cmd_headers;
    port->clbu = 0;
    port->fb = (uint32_t)(uintptr_t)state->recv_fis;
    port->fbu = 0;

    ahci_port_start(port);
    state->initialized = true;
    return true;
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

        if (hba_port->sig != AHCI_SIG_SATA) {
            printf("[AHCI]   skipping non-SATA port %u\n", port);
            continue;
        }

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
        printf("[AHCI] Controller limit reached, skipping %02x:%02x.%u\n",
               dev->bus, dev->device, dev->function);
        return;
    }

    pci_enable_bus_mastering(dev);

    ahci_controller_t *entry = &ahci_controllers[ahci_controller_count++];
    entry->pci_dev = dev;
    entry->abar = abar;
    entry->hba_mem = NULL;
    entry->implemented_ports = 0;
    entry->active_ports = 0;
    memory_set(entry->port_state, 0, sizeof(entry->port_state));

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
