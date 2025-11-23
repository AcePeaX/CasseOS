#include "uhci.h"
#include "uhci_isr.h"
#include "../../pci.h"
#include "../usb.h"
#include "cpu/ports.h"
#include "cpu/timer.h"
#include "libc/mem.h"
#include "libc/function.h"

#define UHCI_PCI_LEGSUP       0xC0
#define UHCI_PCI_LEGCTLSTS    0xC2
#define UHCI_LEGSUP_BIOS_OWNED 0x0001
#define UHCI_LEGSUP_OS_OWNED   0x0002

uint32_t frame_list[1024] __attribute__((aligned(4096)));

uint32_t find_uhci_io_base(pci_device_t *device)
{
    for (int i = 0; i < 6; i++) {
        uint32_t bar = device->bar[i];
        if (bar != 0 && !device->is_memory_mapped[i]) {
            return (uint16_t)(bar & 0xFFFFFFFC);
        }
    }
    return 0;
}

static bool uhci_reset_controller(uint16_t io_base)
{
    port_word_out(io_base + 0x00, 0x0002); // Global Reset
    sleep_ms(10);
    port_word_out(io_base + 0x00, 0x0000);

    uint16_t status = port_word_in(io_base + 0x02);
    if (status & (1 << 5)) { // HCHalted
        return true;
    }
    UHCI_ERR("Controller is NOT halted. Status: 0x%x\n", status);
    return false;
}

static void uhci_initialize_frame_list(void)
{
    for (int i = 0; i < 1024; i++) frame_list[i] = 0x00000001; // T-bit
    UHCI_DBG("Initialized frame list with 1024 T-terminated entries\n");
}

static void uhci_claim_legacy_ownership(pci_device_t *dev)
{
    uint8_t bus = dev->bus, devn = dev->device, fun = dev->function;
    uint16_t legsup = pci_config_read_word(bus, devn, fun, UHCI_PCI_LEGSUP);

    uint16_t new_legsup = legsup;

    if (legsup & UHCI_LEGSUP_BIOS_OWNED) {
        new_legsup |= UHCI_LEGSUP_OS_OWNED;
        pci_config_write_word(bus, devn, fun, UHCI_PCI_LEGSUP, new_legsup);
        for (int i = 0; i < 100; ++i) {
            legsup = pci_config_read_word(bus, devn, fun, UHCI_PCI_LEGSUP);
            if ((legsup & UHCI_LEGSUP_BIOS_OWNED) == 0) break;
            sleep_ms(1);
        }
    }

    /* Leave OS owned asserted and clear BIOS bit/SMI routing. */
    legsup = pci_config_read_word(bus, devn, fun, UHCI_PCI_LEGSUP);
    legsup &= ~UHCI_LEGSUP_BIOS_OWNED;
    legsup |= UHCI_LEGSUP_OS_OWNED;
    pci_config_write_word(bus, devn, fun, UHCI_PCI_LEGSUP, legsup);
    pci_config_write_word(bus, devn, fun, UHCI_PCI_LEGCTLSTS, 0);
}

static bool uhci_set_frame_list_base_address(uint16_t io_base, uint32_t frame_list_phys_addr)
{
    port_dword_out(io_base + 0x08, frame_list_phys_addr);
    uint32_t rd = port_dword_in(io_base + 0x08);
    if (rd == (uintptr_t)frame_list_phys_addr) {
        UHCI_DBG("Frame List set: FLBA=0x%x\n", rd);
        return true;
    }
    UHCI_ERR("Frame List set failed. Expected: 0x%x, Got: 0x%x\n",
             (uintptr_t)frame_list_phys_addr, rd);
    return false;
}

static bool uhci_enable_interrupts(uint16_t io_base)
{
    port_word_out(io_base + 0x04, 0x0006); // HSE | IOC
    uint16_t intr = port_word_in(io_base + 0x04);
    if (intr != 0x0006) {
        UHCI_WARN("Interrupt mask mismatch: wrote 0x0006, read 0x%x\n", intr);
        return false;
    }
    UHCI_DBG("Interrupts enabled (HSE | IOC)\n");
    return true;
}

static bool uhci_start_controller(uint16_t io_base)
{
    port_word_out(io_base + 0x00, 0x0001); // Run/Stop
    uint16_t status = port_word_in(io_base + 0x02);
    if (status != 0) {
        UHCI_WARN("Controller start reported non-zero status: 0x%x\n", status);
    }
    return true;
}

bool uhci_initialize_controller(usb_controller_t *controller)
{
    uint16_t io_base = (uint16_t)controller->base_address;

    uhci_claim_legacy_ownership(controller->pci_device);

    pci_enable_bus_mastering(controller->pci_device);
    UHCI_DBG("Bus mastering enabled\n");

    if (!uhci_reset_controller(io_base)) return false;

    uhci_initialize_frame_list();

    uintptr_t fl_phys = (uintptr_t)&frame_list; // identity map assumption
    if (!uhci_set_frame_list_base_address(io_base, fl_phys)) return false;

    if (!uhci_enable_interrupts(io_base))
        UHCI_WARN("Interrupt enable failed; continuing with polling\n");

    if (!uhci_start_controller(io_base)) {
        UHCI_ERR("Failed to start UHCI controller\n");
        return false;
    }
    uhci_install_isr(controller);

    UHCI_INFO("UHCI Controller initialized at IO base 0x%x\n", io_base);
    return true;
}
