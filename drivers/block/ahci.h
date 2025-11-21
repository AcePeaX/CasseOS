#ifndef DRIVERS_BLOCK_AHCI_H
#define DRIVERS_BLOCK_AHCI_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "drivers/pci.h"

#define AHCI_MAX_CONTROLLERS 8

typedef struct {
    pci_device_t *pci_dev;
    uintptr_t abar;       // MMIO base of the HBA (ABAR)
} ahci_controller_t;

extern ahci_controller_t ahci_controllers[AHCI_MAX_CONTROLLERS];
extern uint32_t ahci_controller_count;

void ahci_init(void);

#endif
