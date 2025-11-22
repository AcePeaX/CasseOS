#ifndef DRIVERS_BLOCK_AHCI_AHCI_H
#define DRIVERS_BLOCK_AHCI_AHCI_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "drivers/pci.h"

#define AHCI_MAX_CONTROLLERS   8
#define AHCI_MAX_PORTS         32
#define AHCI_MAX_COMMAND_SLOTS 32
#define AHCI_MAX_PRDT_ENTRIES  1

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

#define PCI_CLASS_MASS_STORAGE 0x01
#define PCI_SUBCLASS_SATA      0x06
#define PCI_PROG_IF_AHCI       0x01
#define AHCI_BAR_INDEX         5

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
    uint32_t clb;
    uint32_t clbu;
    uint32_t fb;
    uint32_t fbu;
    uint32_t is;
    uint32_t ie;
    uint32_t cmd;
    uint32_t reserved0;
    uint32_t tfd;
    uint32_t sig;
    uint32_t ssts;
    uint32_t sctl;
    uint32_t serr;
    uint32_t sact;
    uint32_t ci;
    uint32_t sntf;
    uint32_t fbs;
    uint32_t reserved1[11];
    uint32_t vendor[4];
} ahci_hba_port_t;

typedef volatile struct {
    uint32_t cap;
    uint32_t ghc;
    uint32_t is;
    uint32_t pi;
    uint32_t vs;
    uint32_t ccc_ctl;
    uint32_t ccc_pts;
    uint32_t em_loc;
    uint32_t em_ctl;
    uint32_t cap2;
    uint32_t bohc;
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
    uintptr_t abar;
    volatile ahci_hba_mem_t *hba_mem;
    uint32_t implemented_ports;
    uint32_t active_ports;
    ahci_port_state_t port_state[AHCI_MAX_PORTS];
} ahci_controller_t;

extern ahci_controller_t ahci_controllers[AHCI_MAX_CONTROLLERS];
extern uint32_t ahci_controller_count;

void ahci_init(void);
void ahci_print_summary(void);

/* Port helpers exposed across translation units */
bool ahci_port_device_present(volatile ahci_hba_port_t *port);
bool ahci_port_initialize(ahci_controller_t *ctrl, uint8_t port_no);
bool ahci_port_identify(ahci_controller_t *ctrl, uint8_t port_no);

#endif
