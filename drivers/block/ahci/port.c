#include "drivers/block/ahci/ahci.h"
#include "drivers/screen.h"
#include "libc/mem.h"

#define AHCI_GENERIC_TIMEOUT 1000000

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

bool ahci_port_device_present(volatile ahci_hba_port_t *port) {
    uint32_t ssts = port->ssts;
    uint32_t det = ssts & 0xF;
    uint32_t spd = (ssts >> 4) & 0xF;
    if (det != 0x3) {
        return false;
    }
    return spd != 0;
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

    for (int i = (int)chars - 1; i >= 0; --i) {
        if (dst[i] == ' ' || dst[i] == '\0') {
            dst[i] = '\0';
        } else {
            break;
        }
    }
}

static bool ahci_run_identify(ahci_port_state_t *state) {
    volatile ahci_hba_port_t *port = state->regs;

    if (!ahci_wait_ready(port)) {
        return false;
    }

    const uint32_t slot = 0;
    ahci_command_header_t *cmd_header = &state->cmd_headers[slot];
    memory_set(cmd_header, 0, sizeof(*cmd_header));
    cmd_header->dw0 = (sizeof(fis_reg_h2d_t) / 4) & 0x1F;
    cmd_header->dw0 |= (1u << 16);
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
    prdt->dbc = (512 - 1) | (1u << 31);

    fis_reg_h2d_t *cmd_fis = (fis_reg_h2d_t *)cmd_table->cfis;
    memory_set(cmd_fis, 0, sizeof(*cmd_fis));
    cmd_fis->fis_type = FIS_TYPE_REG_H2D;
    cmd_fis->pm_and_c = (1u << 7);
    cmd_fis->command = 0xEC;
    cmd_fis->device = (1u << 6);

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

    state->sector_count = (sector_high != 0) ? sector_high : sector_low;
    state->identify_valid = true;
    return true;
}

bool ahci_port_initialize(ahci_controller_t *ctrl, uint8_t port_no) {
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

bool ahci_port_identify(ahci_controller_t *ctrl, uint8_t port_no) {
    ahci_port_state_t *state = &ctrl->port_state[port_no];
    if (!state->initialized || !state->cmd_headers || !state->cmd_table) {
        return false;
    }

    if (!ahci_run_identify(state)) {
        return false;
    }

    printf("[AHCI]   port %02u drive model='%s' sectors=%llu\n",
           port_no,
           state->model,
           (unsigned long long)state->sector_count);
    return true;
}
