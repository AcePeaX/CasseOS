#ifndef DRIVERS_BLOCK_AHCI_H
#define DRIVERS_BLOCK_AHCI_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "drivers/pci.h"

#define AHCI_MAX_CONTROLLERS 8
#define AHCI_MAX_PORTS       32
#define AHCI_MAX_COMMAND_SLOTS 32
#define AHCI_MAX_PRDT_ENTRIES 1

#define AHCI_SIG_SATA   0x00000101
#define AHCI_SIG_SATAPI 0xEB140101
#define AHCI_SIG_SEMB   0xC33C0101
#define AHCI_SIG_PM     0x96690101

#define HBA_PxCMD_ST   (1u << 0)
#define HBA_PxCMD_FRE  (1u << 4)
#define HBA_PxCMD_FR   (1u << 14)
#define HBA_PxCMD_CR   (1u << 15)

#define HBA_PxIS_TFES  (1u << 30)

#define HBA_PxTFD_BSY  (1u << 7)
#define HBA_PxTFD_DRQ  (1u << 3)

#define FIS_TYPE_REG_H2D 0x27

typedef struct {
    uint32_t dba;
    uint32_t dbau;
    uint32_t reserved0;
    uint32_t dbc;
} ahci_prdt_entry_t;

typedef struct {
    uint8_t cfis[64];
    uint8_t acmd[16];
    uint8_t reserved[48];
    ahci_prdt_entry_t prdt_entry[AHCI_MAX_PRDT_ENTRIES];
} ahci_command_table_t;

typedef struct {
    uint32_t dw0;
    uint32_t prdbc;
    uint32_t ctba;
    uint32_t ctbau;
    uint32_t reserved[4];
} ahci_command_header_t;

typedef struct {
    uint8_t fis_type;
    uint8_t pm_and_c;
    uint8_t command;
    uint8_t featurel;
    uint8_t lba0;
    uint8_t lba1;
    uint8_t lba2;
    uint8_t device;
    uint8_t lba3;
    uint8_t lba4;
    uint8_t lba5;
    uint8_t featureh;
    uint8_t countl;
    uint8_t counth;
    uint8_t icc;
    uint8_t control;
    uint8_t reserved[4];
} fis_reg_h2d_t;

typedef volatile struct {
    uint32_t clb;    // 0x00, command list base address
    uint32_t clbu;   // 0x04, command list base address upper 32 bits
    uint32_t fb;     // 0x08, FIS base address
    uint32_t fbu;    // 0x0C, FIS base address upper 32 bits
    uint32_t is;     // 0x10, interrupt status
    uint32_t ie;     // 0x14, interrupt enable
    uint32_t cmd;    // 0x18, command and status
    uint32_t reserved0;
    uint32_t tfd;    // 0x20, task file data
    uint32_t sig;    // 0x24, signature
    uint32_t ssts;   // 0x28, SATA status (SCR0)
    uint32_t sctl;   // 0x2C, SATA control (SCR2)
    uint32_t serr;   // 0x30, SATA error (SCR1)
    uint32_t sact;   // 0x34, SATA active (SCR3)
    uint32_t ci;     // 0x38, command issue
    uint32_t sntf;   // 0x3C, SATA notification
    uint32_t fbs;    // 0x40, FIS-based switch control
    uint32_t reserved1[11];
    uint32_t vendor[4];
} ahci_hba_port_t;

typedef volatile struct {
    uint32_t cap;          // 0x00, host capability
    uint32_t ghc;          // 0x04, global host control
    uint32_t is;           // 0x08, interrupt status
    uint32_t pi;           // 0x0C, port implemented
    uint32_t vs;           // 0x10, version
    uint32_t ccc_ctl;      // 0x14, command completion coalescing control
    uint32_t ccc_pts;      // 0x18, command completion coalescing ports
    uint32_t em_loc;       // 0x1C, enclosure management location
    uint32_t em_ctl;       // 0x20, enclosure management control
    uint32_t cap2;         // 0x24, host capabilities extended
    uint32_t bohc;         // 0x28, BIOS/OS handoff control
    uint8_t  reserved[0xA0 - 0x2C];
    uint8_t  vendor[0x100 - 0xA0];
    ahci_hba_port_t ports[AHCI_MAX_PORTS];
} ahci_hba_mem_t;

typedef struct {
    bool present;
    bool initialized;
    bool identify_valid;
    volatile ahci_hba_port_t *regs;
    ahci_command_header_t *cmd_headers;
    ahci_command_table_t *cmd_table;
    uint8_t *recv_fis;
    char model[41];
    uint64_t sector_count;
} ahci_port_state_t;

typedef struct {
    pci_device_t *pci_dev;
    uintptr_t abar;                    // Physical MMIO base of the HBA (ABAR)
    volatile ahci_hba_mem_t *hba_mem;  // Identity-mapped virtual pointer
    uint32_t implemented_ports;        // Bitmask from PI register
    uint32_t active_ports;             // Bitmask of ports with detected devices
    ahci_port_state_t port_state[AHCI_MAX_PORTS];
} ahci_controller_t;

extern ahci_controller_t ahci_controllers[AHCI_MAX_CONTROLLERS];
extern uint32_t ahci_controller_count;

void ahci_init(void);
void ahci_print_summary(void);

#endif
