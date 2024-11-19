#include "uhci.h"
#include "cpu/ports.h"
#include "../pci.h"
#include "cpu/timer.h"

void uhci_reset_controller(uint16_t io_base) {
    // Set Global Reset bit (bit 2) in the Command Register (offset 0x00)
    port_word_out(io_base + 0x00, 0x0002);
    // Wait for at least 10 microseconds
    for (volatile int i = 0; i < 1000; i++);
    // Clear the Global Reset bit
    port_word_out(io_base + 0x00, 0x0000);
}

uint32_t frame_list[1024] __attribute__((aligned(4096)));

void uhci_set_frame_list_base_address(uint16_t io_base, uint32_t frame_list_phys_addr) {
    // Write to the Frame List Base Address Register (offset 0x08)
    port_dword_out(io_base + 0x08, frame_list_phys_addr);
}

void uhci_initialize_frame_list() {
    for (int i = 0; i < 1024; i++) {
        frame_list[i] = 0x00000001; // Terminate entries
    }
}

void uhci_start_controller(uint16_t io_base) {
    // Set Run/Stop bit (bit 0) in the Command Register
    port_word_out(io_base + 0x00, 0x0001);
}

void uhci_enable_interrupts(uint16_t io_base) {
    // Write to the Interrupt Enable Register (offset 0x04)
    // Enable Host System Error Interrupt, Resume Interrupt, Interrupt on Completion
    port_word_out(io_base + 0x04, 0x0007);
}

void uhci_initialize_controller(usb_controller_t *controller) {
    uint16_t io_base = (uint16_t)controller->base_address;

    pci_enable_bus_mastering(controller->pci_device);

    uhci_reset_controller(io_base);

    uhci_initialize_frame_list();

    // Assuming frame_list is identity-mapped; if not, you need to get the physical address
    uintptr_t frame_list_phys_addr = (uintptr_t) &frame_list;
    uhci_set_frame_list_base_address(io_base, frame_list_phys_addr);

    uhci_start_controller(io_base);

    printf("UHCI Controller initialized at IO base 0x%x\n", io_base);
}

void uhci_reset_port(uint16_t io_base, int port) {
    uint16_t port_addr = io_base + PORT_SC_OFFSET + (port * 2); // Each port uses 2 bytes
    // Reset the port
    port_word_out(port_addr, PORT_RESET);
    // Wait for 50ms
    sleep_ms(50);
    // Clear the reset bit
    port_word_out(port_addr, 0x0000);
    // Wait for the port to recover
    sleep_ms(50);

    // Enable the port
    uint16_t status = port_word_in(port_addr);
    port_word_out(port_addr, status | PORT_ENABLE);

    // Wait for the port to be enabled
    sleep_ms(10);

    // Read the port status
    status = port_word_in(port_addr);
    if (status & PORT_ENABLE && status & PORT_CONNECT_STATUS) {
        printf("Device connected on port %d\n", port);
    } else {
        printf("No device connected on port %d\n", port);
    }
}

void uhci_reset_and_detect_devices(uint16_t io_base) {
    for (int port = 0; port < NUM_PORTS; port++) {
        uhci_reset_port(io_base, port);
    }
}


/*uhci_td_t* allocate_td() {
    uhci_td_t* td = (uhci_td_t*)aligned_alloc(16, sizeof(uhci_td_t));
    if (td) {
        memset(td, 0, sizeof(uhci_td_t));
    }
    return td;
}
uhci_qh_t* allocate_qh() {
    uhci_qh_t* qh = (uhci_qh_t*)aligned_alloc(16, sizeof(uhci_qh_t));
    if (qh) {
        memset(qh, 0, sizeof(uhci_qh_t));
    }
    return qh;
}*/