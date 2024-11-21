#include "cpu/ports.h"
#include "cpu/timer.h"
#include "libc/mem.h"
#include "libc/function.h"
#include <stddef.h>

#include "uhci.h"
#include "../pci.h"

uint32_t find_uhci_io_base(pci_device_t* device) {
    for (int i = 0; i < 6; i++) { // Check all 6 BARs
        uint32_t bar = device->bar[i];
        if (bar!=0 && !device->is_memory_mapped[i]) { // Check if it's an I/O space
            return (uint16_t)(bar & 0xFFFFFFFC); // Return the base address
        }
    }
    return 0; // No valid BAR found
}

bool uhci_reset_controller(uint16_t io_base) {

    // Set Global Reset bit (bit 2) in the Command Register (offset 0x00)
    port_word_out(io_base + 0x00, 0x0002);
    // Wait for at least 10 microseconds
    sleep_ms(10);
    // Clear the Global Reset bit
    port_word_out(io_base + 0x00, 0x0000);


    // Check USB Status Register
    uint16_t status = port_word_in(io_base + 0x02);
    if (status & (1 << 5)) {
        //printf("Controller successfully halted.\n");
        return true;
    } else {
        printf("Controller is NOT halted. Status: 0x%x\n", status);
        return false;
    }
}

uint32_t frame_list[1024] __attribute__((aligned(4096)));

bool uhci_set_frame_list_base_address(uint16_t io_base, uint32_t frame_list_phys_addr) {
    // Write to the Frame List Base Address Register (offset 0x08)
    port_dword_out(io_base + 0x08, frame_list_phys_addr);
    uint32_t frame_list_address = port_dword_in(io_base + 0x08); // Read Frame List Base Address Register
    if (frame_list_address == (uintptr_t)frame_list_phys_addr) {
        //printf("Frame List successfully set: 0x%x and 0x%x\n", frame_list_address, frame_list_phys_addr);
        return true;
    } else {
        printf("Frame List set failed. Expected: 0x%x, Got: 0x%x\n",
                (uintptr_t)frame_list_phys_addr, frame_list_address);
    }
    return false;
}

void uhci_initialize_frame_list() {
    for (int i = 0; i < 1024; i++) {
        frame_list[i] = 0x00000001; // Terminate entries
    }
}

bool uhci_start_controller(uint16_t io_base) {
    // Set Run/Stop bit (bit 0) in the Command Register
    port_word_out(io_base + 0x00, 0x0001);
    uint16_t status = port_word_in(io_base + 0x02); // Read Status Register
    if(status!=0){
        printf("The controller was not correctly started!");
        return false;
    }
    return true;
}

bool uhci_enable_interrupts(uint16_t io_base) {
    // Write to the Interrupt Enable Register (offset 0x04)
    // Enable Host System Error Interrupt, Resume Interrupt, Interrupt on Completion
    port_word_out(io_base + 0x04, 0x0006);
    uint32_t intr = port_word_in(io_base + 0x04);
    if(intr != 0x06){
        return false;
    }
    return true;
}

bool uhci_initialize_controller(usb_controller_t *controller) {
    uint16_t io_base = (uint16_t)controller->base_address;

    pci_enable_bus_mastering(controller->pci_device);


    if(!uhci_reset_controller(io_base)){
        return false;
    }

    uhci_initialize_frame_list();

    // Assuming frame_list is identity-mapped; if not, you need to get the physical address
    uintptr_t frame_list_phys_addr = (uintptr_t) &frame_list;
    if(!uhci_set_frame_list_base_address(io_base, frame_list_phys_addr)){
        return false;
    }

    if(!uhci_enable_interrupts(io_base)){
        return false;
    }

    if(!uhci_start_controller(io_base)){
        return false;
    }

    printf("UHCI Controller initialized at IO base 0x%x\n", io_base);

    return true;
}

void uhci_reset_port(uint16_t io_base, int port) {
    uint16_t port_addr = io_base + PORT_SC_OFFSET + (port * 2); // Each port uses 2 bytes
    // Reset the port
    port_word_out(port_addr, PORT_RESET);
    // Wait for 50ms
    sleep_ms(50);
    // Clear the reset bit
    port_word_out(port_addr, port_word_in(port_addr) & ~PORT_RESET);
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


uhci_td_t* allocate_td() {
    uhci_td_t* td = (uhci_td_t*)aligned_alloc(16, sizeof(uhci_td_t));
    if (td == NULL) {
        // Handle allocation failure
        return NULL;
    }
    memory_set(td, 0, sizeof(uhci_td_t));
    return td;
}

uhci_qh_t* allocate_qh() {
    uhci_qh_t* qh = (uhci_qh_t*)aligned_alloc(16, sizeof(uhci_qh_t));
    if (qh == NULL) {
        // Handle allocation failure
        return NULL;
    }
    memory_set(qh, 0, sizeof(uhci_qh_t));
    return qh;
}

usb_setup_packet_t* allocate_setup_packet() {
    usb_setup_packet_t* packet = (usb_setup_packet_t*)aligned_alloc(16, sizeof(usb_setup_packet_t));
    if (packet == NULL) {
        printf("Failed to allocate Setup Packet\n");
        return NULL;
    }
    memory_set(packet, 0, sizeof(usb_setup_packet_t));
    return packet;
}

void free_td(uhci_td_t* td) {
    aligned_free(td);
}

void free_qh(uhci_qh_t* qh) {
    aligned_free(qh);
}

void free_setup_packet(usb_setup_packet_t* setup_packet) {
    aligned_free(setup_packet);
}

int uhci_wait_for_transfer_complete(uhci_td_t* td) {
    int timeout = 3000; // Timeout in milliseconds
    while ((td->control_status & 0x800000) && timeout > 0) {
        sleep_ms(1); // Wait for 1 millisecond
        timeout--;
    }
    if (timeout == 0) {
        printf("Transfer timeout\n");
        return -1; // Indicate failure
    }
    if (td->control_status & (1 << 22)) {
        printf("Transfer stalled\n");
        return -2; // Indicate failure
    }
    if (td->control_status & (1 << 21)) {
        printf("Data Buffer Error\n");
        return -3; // Indicate failure
    }
    if (td->control_status & (1 << 20)) {
        printf("Babble Detected\n");
        return -4; // Indicate failure
    }
    if (td->control_status & (1 << 19)) {
        printf("NAK Received\n");
        return -5; // Indicate failure
    }
    if (td->control_status & (1 << 18)) {
        printf("CRC/Timeout Error\n");
        return -6; // Indicate failure
    }
    if (td->control_status & (1 << 17)) {
        printf("Bit Stuff Error\n");
        return -7; // Indicate failure
    }
    return 1;
}

int uhci_set_device_address(uint16_t io_base, uint8_t port, uint8_t new_address) {
    
    // Create the setup packet for SET_ADDRESS
    usb_setup_packet_t* setup_packet = allocate_setup_packet();
    if (setup_packet == NULL) {
        return 0;
    }

    // Initialize the setup packet
    setup_packet->bmRequestType = 0x00; // Host to Device, Standard, Device
    setup_packet->bRequest = 0x05;      // SET_ADDRESS
    setup_packet->wValue = new_address;
    setup_packet->wIndex = 0;
    setup_packet->wLength = 0;
    
    UNUSED(port);

    // Allocate TDs and QH
    uhci_td_t* setup_td = allocate_td();
    uhci_td_t* status_td = allocate_td();
    uhci_qh_t* qh = allocate_qh();


    if (!setup_td || !status_td || !qh) {
        printf("Error allocating TDs/QH\n");
        return 0;
    }

    // Initialize Setup TD
    setup_td->link_pointer = get_physical_address(status_td) | 0x04; // T-bit 0
    setup_td->control_status = 0x800000; // Active
    setup_td->token = (0x2D) | (0 << 8) | (0 << 15) | (7 << 21); // PID_SETUP, Device Address 0, Endpoint 0, Data Length 8
    setup_td->buffer_pointer = get_physical_address(setup_packet);

    // Initialize Status TD
    status_td->link_pointer = 0x00000001; // Terminate
    status_td->control_status = 0x800000;     // Active
    status_td->token = (0x69) | (0 << 8) | (0 << 15) | (0 << 16); // PID_IN, Device Address 0, Endpoint 0, Data Length 0
    status_td->buffer_pointer = 0;



    //printf("setup_td: %d, Ob%b, %d, %d\n", setup_td->link_pointer, setup_td->control_status, setup_td->token, setup_td->buffer_pointer);
    //printf("status_td: %d, Ob%b, %d, %d\n", status_td->link_pointer, status_td->control_status, status_td->token, status_td->buffer_pointer);
    
    // Initialize QH
    qh->horizontal_link_pointer = 0x00000001; // Terminate
    qh->vertical_link_pointer = get_physical_address(setup_td);

    uint16_t current_frame = port_word_in(io_base + 0x6);

    // For example, read the Frame Number Register to synchronize
    uint16_t frame_number = current_frame+1;//port_word_in(io_base + 0x06) & 0x7FF; // 11 bits
    frame_list[(frame_number) % 1024] = get_physical_address(qh) | 0x00000002;

    uint16_t command = port_word_in(io_base + 0x00);
    if (!(command & 0x01)) {
        printf("Controller is not running\n");
        return 0;
    }

    // Wait for transfer completion
    if(uhci_wait_for_transfer_complete(setup_td)!=1){
        return 0;
    }

    uint16_t status = port_word_in(io_base + 0x02);
    if (status & 0x02) {
        printf("USB Error Interrupt occurred\n");
        // Clear the USBERRINT bit
        port_word_out(io_base + 0x02, 0x02);
    }

    // Check if the transfer was successful
    if (setup_td->control_status & 0x400000) {
        printf("Error in SET_ADDRESS transfer\n");
        printf("TD SetUp Status: 0x%x\n", setup_td->control_status);
        printf("TD Status: 0x%x\n", status_td->control_status);
        // Clean up
        free_td(setup_td);
        free_td(status_td);
        free_qh(qh);
        free_setup_packet(setup_packet);
        return 0;
    } else {
        printf("Device address set to %d\n", new_address);
    }

    // Clean up
    frame_list[frame_number % 1024] = 0x00000001; // Terminate
    free_td(setup_td);
    free_td(status_td);
    free_qh(qh);
    free_setup_packet(setup_packet);

    sleep_ms(10); // Wait for the device to process the new address

    return 1; // Success
}


int uhci_get_device_descriptor(uint16_t io_base, uint8_t device_address, usb_device_descriptor_t* device_desc) {
    // Create the setup packet for GET_DESCRIPTOR
    usb_setup_packet_t setup_packet = {
        .bmRequestType = 0x80, // Device to Host, Standard, Device
        .bRequest = 0x06,      // GET_DESCRIPTOR
        .wValue = (USB_DESC_TYPE_DEVICE << 8) | 0x00, // Descriptor Type and Index
        .wIndex = 0,
        .wLength = sizeof(usb_device_descriptor_t)
    };

    UNUSED(io_base);

    // Allocate TDs and QH
    uhci_td_t* setup_td = allocate_td();
    uhci_td_t* data_td = allocate_td();
    uhci_td_t* status_td = allocate_td();
    uhci_qh_t* qh = allocate_qh();

    if (!setup_td || !data_td || !status_td || !qh) {
        printf("Error allocating TDs/QH\n");
        return 0;
    }

    // Initialize Setup TD
    setup_td->link_pointer = get_physical_address(data_td) | 0x04;
    setup_td->control_status = 0x80; // Active
    setup_td->token = (0x2D) | (device_address << 8) | (0 << 15) | (8 << 16); // PID_SETUP
    setup_td->buffer_pointer = get_physical_address(&setup_packet);

    // Initialize Data TD
    data_td->link_pointer = get_physical_address(status_td) | 0x04;
    data_td->control_status = 0x80; // Active
    data_td->token = (0x69) | (device_address << 8) | (1 << 19) | (sizeof(usb_device_descriptor_t) << 16); // PID_IN, Data Toggle 1
    data_td->buffer_pointer = get_physical_address(device_desc);

    // Initialize Status TD
    status_td->link_pointer = 0x00000001; // Terminate
    status_td->control_status = 0x80;     // Active
    status_td->token = (0xE1) | (device_address << 8) | (1 << 19) | (0 << 16); // PID_OUT, Data Toggle 1
    status_td->buffer_pointer = 0;

    // Initialize QH
    qh->horizontal_link_pointer = 0x00000001; // Terminate
    qh->vertical_link_pointer = get_physical_address(setup_td);


    // Insert QH into Frame List
    uint16_t frame_number = port_word_in(io_base + 0x06) & 0x7FF; // 11 bits
    frame_list[frame_number % 1024] = get_physical_address(qh) | 0x00000002; // Set QH bit

    // Wait for transfer completion
    uhci_wait_for_transfer_complete(data_td);

    // Check if the transfer was successful
    if (data_td->control_status & 0x400000) {
        printf("Error in GET_DESCRIPTOR transfer\n");
        // Clean up
        free_td(setup_td);
        free_td(data_td);
        free_td(status_td);
        free_qh(qh);
        return 0;
    } else {
        printf("Device Descriptor retrieved\n");
    }

    // Clean up
    frame_list[frame_number % 1024] = 0x00000001; // Terminate
    free_td(setup_td);
    free_td(data_td);
    free_td(status_td);
    free_qh(qh);

    return 1; // Success
}




int uhci_set_configuration(uint16_t io_base, uint8_t device_address, uint8_t configuration_value) {
    // Create the setup packet for SET_CONFIGURATION
    usb_setup_packet_t setup_packet = {
        .bmRequestType = 0x00, // Host to Device, Standard, Device
        .bRequest = 0x09,      // SET_CONFIGURATION
        .wValue = configuration_value,
        .wIndex = 0,
        .wLength = 0
    };

    UNUSED(io_base);

    // Allocate TDs and QH
    uhci_td_t* setup_td = allocate_td();
    uhci_td_t* status_td = allocate_td();
    uhci_qh_t* qh = allocate_qh();

    if (!setup_td || !status_td || !qh) {
        printf("Error allocating TDs/QH\n");
        return 0;
    }

    // Initialize Setup TD
    setup_td->link_pointer = get_physical_address(status_td) | 0x04;
    setup_td->control_status = 0x80; // Active
    setup_td->token = (0x2D) | (device_address << 8) | (0 << 15) | (8 << 16); // PID_SETUP
    setup_td->buffer_pointer = get_physical_address(&setup_packet);

    // Initialize Status TD
    status_td->link_pointer = 0x00000001; // Terminate
    status_td->control_status = 0x80;     // Active
    status_td->token = (0x69) | (device_address << 8) | (1 << 19) | (0 << 16); // PID_IN, Data Toggle 1
    status_td->buffer_pointer = 0;

    // Initialize QH
    qh->horizontal_link_pointer = 0x00000001; // Terminate
    qh->vertical_link_pointer = get_physical_address(setup_td);

    // Insert QH into Frame List
    uint16_t frame_number = port_word_in(io_base + 0x06) & 0x7FF; // 11 bits
    frame_list[frame_number % 1024] = get_physical_address(qh) | 0x00000002; // Set QH bit

    // Wait for transfer completion
    uhci_wait_for_transfer_complete(status_td);

    // Check if the transfer was successful
    if (status_td->control_status & 0x40) {
        printf("Error in SET_CONFIGURATION transfer\n");
        // Clean up
        free_td(setup_td);
        free_td(status_td);
        free_qh(qh);
        return 0;
    } else {
        printf("Device configured with configuration %d\n", configuration_value);
    }

    // Clean up
    frame_list[frame_number % 1024] = 0x00000001; // Terminate
    free_td(setup_td);
    free_td(status_td);
    free_qh(qh);

    return 1; // Success
}



void uhci_enumerate_device(uint16_t io_base, int port) {
    uint8_t device_address = port + 1; // Assign a unique address per port

    // Step 1: Reset and detect device
    uhci_reset_port(io_base, port);
    uint16_t port_status = port_word_in(io_base + PORT_SC_OFFSET + (port * 2));
    if (!((port_status & PORT_ENABLE) && (port_status & PORT_CONNECT_STATUS))) {
        printf("No device to enumerate on port %d\n", port);
        return;
    }

    // Step 2: Set device address
    if (!uhci_set_device_address(io_base, port, device_address)) {
        printf("Failed to set device address on port %d\n", port);
        return;
    }

    // Step 3: Get device descriptor
    usb_device_descriptor_t device_desc;
    if (!uhci_get_device_descriptor(io_base, device_address, &device_desc)) {
        printf("Failed to get device descriptor on port %d\n", port);
        return;
    }


    // Step 4: Set configuration
    if (!uhci_set_configuration(io_base, device_address, 1)) {
        printf("Failed to set configuration on port %d\n", port);
        return;
    }

    // Print device information
    printf("Device on port %d:\n", port);
    printf("  Vendor ID: 0x%x\n", device_desc.vendor_id);
    printf("  Product ID: 0x%x\n", device_desc.product_id);
    printf("  Class: 0x%x\n", device_desc.device_class);
    printf("  Subclass: 0x%x\n", device_desc.device_subclass);
    printf("  Protocol: 0x%x\n", device_desc.device_protocol);

    // Additional steps:
    // - Parse configuration descriptor
    // - Initialize drivers for specific devices (e.g., keyboard driver)
}

void uhci_enumerate_devices(usb_controller_t* controller) {
    uint16_t io_base = (uint16_t)(controller->base_address);
    for (int port = 0; port < NUM_PORTS; port++) {
        uhci_enumerate_device(io_base, port);
    }
}

