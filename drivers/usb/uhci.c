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
    status_td->token = (0x69) | (0 << 8) | (0 << 15) | (0 << 21); // PID_IN, Device Address 0, Endpoint 0, Data Length 0
    status_td->buffer_pointer = 0;

    // Initialize QH
    qh->horizontal_link_pointer = 0x00000001; // Terminate
    qh->vertical_link_pointer = get_physical_address(setup_td);

    uint16_t current_frame = port_word_in(io_base + 0x6) & 0x7FF;

    // For example, read the Frame Number Register to synchronize
    uint16_t frame_number = current_frame+1;//port_word_in(io_base + 0x06) & 0x7FF; // 11 bits
    frame_list[(frame_number) % 1024] = get_physical_address(qh) | 0x00000002;
    frame_list[(frame_number+1) % 1024] = get_physical_address(qh) | 0x00000002;

    uint16_t command = port_word_in(io_base + 0x00);
    if (!(command & 0x01)) {
        printf("Controller is not running\n");
        return 0;
    }

    // Wait for transfer completion
    if(uhci_wait_for_transfer_complete(status_td)!=1){
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
    }

    // Clean up
    frame_list[frame_number % 1024] = 0x00000001; // Terminate
    frame_list[(frame_number+1) % 1024] = 0x00000001; // Terminate
    free_td(setup_td);
    free_td(status_td);
    free_qh(qh);
    free_setup_packet(setup_packet);

    sleep_ms(10); // Wait for the device to process the new address

    return 1; // Success
}

int uhci_get_device_descriptor(uint16_t io_base, uint8_t device_address, usb_device_descriptor_t* device_desc) {
    // Create the setup packet for GET_DESCRIPTOR
    usb_setup_packet_t* setup_packet = allocate_setup_packet();
    if (setup_packet == NULL) {
        return 0;
    }

    // Initialize the setup packet
    setup_packet->bmRequestType = 0x80; // Host to Device, Standard, Device
    setup_packet->bRequest = 0x06;      // SET_ADDRESS
    setup_packet->wValue = (USB_DESC_TYPE_DEVICE << 8) | 0x00;
    setup_packet->wIndex = 0;
    setup_packet->wLength = sizeof(usb_device_descriptor_t);

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
    setup_td->control_status = 0x800000; // Active
    setup_td->token = (0x2D) | (device_address << 8) | (0 << 15) | (7 << 21); // PID_SETUP
    setup_td->buffer_pointer = get_physical_address(setup_packet);

    // Initialize Data TD
    data_td->link_pointer = get_physical_address(status_td) | 0x04;
    data_td->control_status = 0x800000; // Active
    data_td->token = (0x69) | (device_address << 8) | (1 << 19) | ((sizeof(usb_device_descriptor_t) - 1) << 21); // PID_IN, Data Toggle 1
    data_td->buffer_pointer = get_physical_address(device_desc);

    // Initialize Status TD
    status_td->link_pointer = 0x00000001; // Terminate
    status_td->control_status = 0x800000;     // Active
    status_td->token = (0xE1) | (device_address << 8) | (1 << 19) | (0 << 21); // PID_OUT, Data Toggle 1
    status_td->buffer_pointer = 0;

    // Initialize QH
    qh->horizontal_link_pointer = 0x00000001; // Terminate
    qh->vertical_link_pointer = get_physical_address(setup_td);


    // Insert QH into Frame List
    uint16_t frame_number = (port_word_in(io_base + 0x06) & 0x7FF)+1; // 11 bits
    frame_list[(frame_number) % 1024] = get_physical_address(qh) | 0x00000002; // Set QH bit
    frame_list[(frame_number+1) % 1024] = get_physical_address(qh) | 0x00000002; // Set QH bit

    // Wait for transfer completion
    if(uhci_wait_for_transfer_complete(status_td)!=1){
        return 0;
    }

    // Check if the transfer was successful
    if (data_td->control_status & 0x400000) {
        printf("Error in GET_DESCRIPTOR transfer\n");
        // Clean up
        free_td(setup_td);
        free_td(data_td);
        free_td(status_td);
        free_qh(qh);
        return 0;
    }

    // Clean up
    frame_list[(frame_number) % 1024] = 0x00000001; // Terminate
    frame_list[(frame_number+1) % 1024] = 0x00000001; // Terminate
    free_td(setup_td);
    free_td(data_td);
    free_td(status_td);
    free_qh(qh);

    return 1; // Success
}

int uhci_set_configuration(uint16_t io_base, uint8_t device_address, uint8_t configuration_value) {
    // Create the setup packet for GET_DESCRIPTOR
    usb_setup_packet_t* setup_packet = allocate_setup_packet();
    if (setup_packet == NULL) {
        return 0;
    }

    // Initialize the setup packet
    setup_packet->bmRequestType = 0x00; // Host to Device, Standard, Device
    setup_packet->bRequest = 0x09;      // SET_CONFIGURATION
    setup_packet->wValue = configuration_value;
    setup_packet->wIndex = 0;
    setup_packet->wLength = 0;

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
    setup_td->control_status = 0x800000; // Active
    setup_td->token = (0x2D) | (device_address << 8) | (0 << 15) | (7 << 21); // PID_SETUP
    setup_td->buffer_pointer = get_physical_address(setup_packet);

    // Initialize Status TD
    status_td->link_pointer = 0x00000001; // Terminate
    status_td->control_status = 0x800000;     // Active
    status_td->token = (0x69) | (device_address << 8) | (1 << 19) | (0 << 21); // PID_IN, Data Toggle 1
    status_td->buffer_pointer = 0;

    // Initialize QH
    qh->horizontal_link_pointer = 0x00000001; // Terminate
    qh->vertical_link_pointer = get_physical_address(setup_td);

    // Insert QH into Frame List
    uint16_t frame_number = (port_word_in(io_base + 0x06) & 0x7FF) + 1; // 11 bits
    frame_list[frame_number % 1024] = get_physical_address(qh) | 0x00000002; // Set QH bit
    frame_list[(frame_number+1) % 1024] = get_physical_address(qh) | 0x00000002; // Set QH bit

    // Wait for transfer completion
    if(uhci_wait_for_transfer_complete(status_td)!=1){
        return 0;
    }

    // Check if the transfer was successful
    if (status_td->control_status & (1 << 22)) {
        printf("Error in SET_CONFIGURATION transfer\n");
        // Clean up
        free_td(setup_td);
        free_td(status_td);
        free_qh(qh);
        return 0;
    }

    // Clean up
    frame_list[frame_number % 1024] = 0x00000001; // Terminate
    frame_list[(frame_number+1) % 1024] = 0x00000001; // Terminate
    free_td(setup_td);
    free_td(status_td);
    free_qh(qh);

    return 1; // Success
}

int uhci_get_configuration_descriptor(uint16_t io_base, uint8_t device_address, usb_configuration_descriptor_t* config_desc) {
    // Create the setup packet for GET_DESCRIPTOR
    usb_setup_packet_t* setup_packet = allocate_setup_packet();
    if (setup_packet == NULL) {
        return 0;
    }

    // Initialize the setup packet
    setup_packet->bmRequestType = 0x80; // Host to Device, Standard, Device
    setup_packet->bRequest = 0x06;      // GET_DESCRIPTOR
    setup_packet->wValue = (USB_DESC_TYPE_CONFIGURATION << 8) | 0x00; // Configuration descriptor type
    setup_packet->wIndex = 0;
    setup_packet->wLength = sizeof(usb_configuration_descriptor_t);

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
    setup_td->control_status = 0x800000; // Active
    setup_td->token = (0x2D) | (device_address << 8) | (0 << 15) | (7 << 21); // PID_SETUP
    setup_td->buffer_pointer = get_physical_address(setup_packet);

    // Initialize Data TD
    data_td->link_pointer = get_physical_address(status_td) | 0x04;
    data_td->control_status = 0x800000; // Active
    data_td->token = (0x69) | (device_address << 8) | (1 << 19) | ((sizeof(usb_configuration_descriptor_t)-1) << 21); // PID_IN, Data Toggle 1
    data_td->buffer_pointer = get_physical_address(config_desc);

    // Initialize Status TD
    status_td->link_pointer = 0x00000001; // Terminate
    status_td->control_status = 0x800000;     // Active
    status_td->token = (0xE1) | (device_address << 8) | (1 << 19) | (0 << 21); // PID_OUT, Data Toggle 1
    status_td->buffer_pointer = 0;

    // Initialize QH
    qh->horizontal_link_pointer = 0x00000001; // Terminate
    qh->vertical_link_pointer = get_physical_address(setup_td);

    // Insert QH into Frame List
    uint16_t frame_number = port_word_in(io_base + 0x06) & 0x7FF; // 11 bits
    frame_list[frame_number % 1024] = get_physical_address(qh) | 0x00000002; // Set QH bit

    // Wait for transfer completion
    if (uhci_wait_for_transfer_complete(data_td) != 1) {
        printf("Error in GET_DESCRIPTOR transfer for configuration descriptor\n");
        free_td(setup_td);
        free_td(data_td);
        free_td(status_td);
        free_qh(qh);
        return false;
    }

    // Clean up
    frame_list[frame_number % 1024] = 0x00000001; // Terminate
    free_td(setup_td);
    free_td(data_td);
    free_td(status_td);
    free_qh(qh);

    return 1;
}

int uhci_get_full_configuration_descriptor(uint16_t io_base, uint8_t device_address,
                                           uint8_t *buffer, uint16_t total_length) {
    // Create the setup packet for GET_DESCRIPTOR
    usb_setup_packet_t* setup_packet = allocate_setup_packet();
    if (!setup_packet) return 0;

    // Initialize the setup packet
    setup_packet->bmRequestType = 0x80; // Device-to-Host, Standard, Device
    setup_packet->bRequest = 0x06;      // GET_DESCRIPTOR
    setup_packet->wValue = (USB_DESC_TYPE_CONFIGURATION << 8) | 0x00;
    setup_packet->wIndex = 0;
    setup_packet->wLength = total_length;

    // Allocate TDs and QH
    uhci_td_t* setup_td = allocate_td();
    uhci_td_t* data_td = allocate_td();
    uhci_td_t* status_td = allocate_td();
    uhci_qh_t* qh = allocate_qh();

    if (!setup_td || !data_td || !status_td || !qh) {
        printf("Allocation failed for full config descriptor transfer\n");
        return 0;
    }

    // Setup TD
    setup_td->link_pointer = get_physical_address(data_td) | 0x04;
    setup_td->control_status = 0x800000;
    setup_td->token = (0x2D) | (device_address << 8) | (0 << 15) | (7 << 21);
    setup_td->buffer_pointer = get_physical_address(setup_packet);

    // Data TD
    data_td->link_pointer = get_physical_address(status_td) | 0x04;
    data_td->control_status = 0x800000;
    data_td->token = (0x69) | (device_address << 8) | (1 << 19) | ((total_length - 1) << 21);
    data_td->buffer_pointer = get_physical_address(buffer);

    // Status TD
    status_td->link_pointer = 0x00000001;
    status_td->control_status = 0x800000;
    status_td->token = (0xE1) | (device_address << 8) | (1 << 19) | (0 << 21);
    status_td->buffer_pointer = 0;

    // Queue Head
    qh->horizontal_link_pointer = 0x00000001;
    qh->vertical_link_pointer = get_physical_address(setup_td);

    // Insert QH into frame list
    uint16_t frame_number = (port_word_in(io_base + 0x06) & 0x7FF);
    frame_list[frame_number % 1024] = get_physical_address(qh) | 0x00000002;

    // Wait for transfer to complete
    if (uhci_wait_for_transfer_complete(data_td) != 1) {
        printf("GET_DESCRIPTOR (full config) failed\n");
        free_td(setup_td); free_td(data_td); free_td(status_td); free_qh(qh);
        free_setup_packet(setup_packet);
        return 0;
    }

    // Cleanup
    frame_list[frame_number % 1024] = 0x00000001;
    free_td(setup_td); free_td(data_td); free_td(status_td); free_qh(qh);
    free_setup_packet(setup_packet);

    return 1; // success
}


int uhci_get_interface_descriptor(uint16_t io_base, uint8_t device_address, usb_interface_descriptor_t* interface_desc) {
    // Create the setup packet for GET_DESCRIPTOR
    usb_setup_packet_t* setup_packet = allocate_setup_packet();
    if (setup_packet == NULL) {
        return 0;
    }

    // Initialize the setup packet
    setup_packet->bmRequestType = 0x80; // Device to Host, Standard, Device
    setup_packet->bRequest = 0x06;      // GET_DESCRIPTOR
    setup_packet->wValue = (USB_DESC_TYPE_INTERFACE << 8) | 0x00; // Interface descriptor type
    setup_packet->wIndex = 0;
    setup_packet->wLength = sizeof(usb_interface_descriptor_t);

    // Allocate TDs and QH
    uhci_td_t* setup_td = allocate_td();
    uhci_td_t* data_td = allocate_td();
    uhci_td_t* status_td = allocate_td();
    uhci_qh_t* qh = allocate_qh();

    if (!setup_td || !data_td || !status_td || !qh) {
        printf("Error allocating TDs/QH\n");
        free_setup_packet(setup_packet);
        return 0;
    }

    // Initialize Setup TD
    setup_td->link_pointer = get_physical_address(data_td) | 0x04;
    setup_td->control_status = 0x800000; // Active
    setup_td->token = (0x2D) | (device_address << 8) | (0 << 15) | (7 << 21); // PID_SETUP
    setup_td->buffer_pointer = get_physical_address(setup_packet);

    // Initialize Data TD
    data_td->link_pointer = get_physical_address(status_td) | 0x04;
    data_td->control_status = 0x800000; // Active
    data_td->token = (0x69) | (device_address << 8) | (1 << 19) | ((sizeof(usb_interface_descriptor_t) - 1) << 21); // PID_IN, Data Toggle 1
    data_td->buffer_pointer = get_physical_address(interface_desc);

    // Initialize Status TD
    status_td->link_pointer = 0x00000001; // Terminate
    status_td->control_status = 0x800000;     // Active
    status_td->token = (0xE1) | (device_address << 8) | (1 << 19) | (0 << 21); // PID_OUT, Data Toggle 1
    status_td->buffer_pointer = 0;

    // Initialize QH
    qh->horizontal_link_pointer = 0x00000001; // Terminate
    qh->vertical_link_pointer = get_physical_address(setup_td);

    // Insert QH into Frame List
    uint16_t frame_number = (port_word_in(io_base + 0x06) & 0x7FF) + 1; // 11 bits
    frame_list[frame_number % 1024] = get_physical_address(qh) | 0x00000002; // Set QH bit

    // Wait for transfer completion
    if (uhci_wait_for_transfer_complete(setup_td) != 1) {
        printf("Error in GET_DESCRIPTOR transfer for interface descriptor\n");
        free_td(setup_td);
        free_td(data_td);
        free_td(status_td);
        free_qh(qh);
        free_setup_packet(setup_packet);
        return 0;
    }

    // Clean up
    frame_list[frame_number % 1024] = 0x00000001; // Terminate
    free_td(setup_td);
    free_td(data_td);
    free_td(status_td);
    free_qh(qh);
    free_setup_packet(setup_packet);

    return 1; // Success
}

// Pretty-print the full Configuration descriptor blob (Config + Interface + HID + Endpoint)
static void usb_dump_configuration_blob(const uint8_t *buf, uint16_t total_len) {
    uint16_t off = 0;

    printf("----- USB Configuration Blob (%u bytes) -----\n", (unsigned int)total_len);

    while (off + 2 <= total_len) {
        uint8_t bLength = buf[off];
        uint8_t bType   = buf[off + 1];

        if (bLength == 0) {
            printf("  [!] bLength == 0 at offset %u, stopping.\n", (unsigned int)off);
            break;
        }
        if (off + bLength > total_len) {
            printf("  [!] Descriptor overruns buffer: off=%u len=%u total=%u\n",
                   (unsigned int)off, (unsigned int)bLength, (unsigned int)total_len);
            break;
        }

        switch (bType) {
            case USB_DESC_TYPE_CONFIGURATION: {
                const usb_configuration_descriptor_t *cd =
                    (const usb_configuration_descriptor_t *)&buf[off];
                printf("\nCONFIGURATION @%u\n", (unsigned int)off);
                printf("  total_length=%u  num_interfaces=%u\n",
                       (unsigned int)cd->total_length, cd->num_interfaces);
                printf("  value=%u  index=%u  attrs=0x%x  max_power=%u\n",
                       cd->configuration_value, cd->configuration_index,
                       cd->attributes, cd->max_power);
                break;
            }
            case USB_DESC_TYPE_INTERFACE: {
                const usb_interface_descriptor_t *id =
                    (const usb_interface_descriptor_t *)&buf[off];
                printf("\nINTERFACE @%u\n", (unsigned int)off);
                printf("  num=%u  alt=%u  eps=%u\n",
                       id->interface_number, id->alternate_setting, id->num_endpoints);
                printf("  class=0x%x  subcls=0x%x  proto=0x%x  idx=%u\n",
                       id->interface_class, id->interface_subclass,
                       id->interface_protocol, id->interface_index);
                break;
            }
            case USB_DESC_TYPE_HID: {
                const usb_hid_descriptor_t *hid =
                    (const usb_hid_descriptor_t *)&buf[off];
                printf("\nHID @%u\n", (unsigned int)off);
                printf("  ver=0x%x  country=%u  descs=%u\n",
                       hid->hid_version, hid->country_code, hid->num_descriptors);
                printf("  report_type=0x%x  report_len=%u\n",
                       hid->report_type, hid->report_length);
                break;
            }
            case USB_DESC_TYPE_ENDPOINT: {
                const usb_endpoint_descriptor_t *ep =
                    (const usb_endpoint_descriptor_t *)&buf[off];
                printf("\nENDPOINT @%u\n", (unsigned int)off);
                uint8_t dir = (ep->endpoint_address & 0x80) ? 1 : 0;
                printf("  addr=0x%x  dir=%s  attr=0x%x\n",
                       ep->endpoint_address, dir ? "IN" : "OUT", ep->attributes);
                printf("  max_packet=%u  interval=%u\n",
                       ep->max_packet_size, ep->interval);
                break;
            }
            default:
                printf("\nUNKNOWN DESC type=0x%x len=%u @%u\n",
                       bType, bLength, (unsigned int)off);
                break;
        }

        sleep_ms(1000);

        off += bLength;
    }

    printf("\n----- End of Configuration Blob -----\n");
}

// Robust two-pass parser.
// Returns 1 on success, 0 on failure.
// Fills dev->config_descriptor, dev->interface_descriptor (chosen IF), and dev->endpoint_descriptors[0] (INT IN).
static int usb_parse_config_blob_into_device(const uint8_t *buf, uint16_t total_len, usb_device_t *dev) {
    if (!buf || !dev) return 0;
    if (total_len < sizeof(usb_configuration_descriptor_t)) return 0;

    // Copy the 9-byte configuration header so you can read configuration_value, num_interfaces, etc.
    memory_copy(&dev->config_descriptor, buf, sizeof(usb_configuration_descriptor_t));

    // ---------- Pass 1: choose interface ----------
    uint16_t off = 0;
    int have_any_if = 0;
    int chose_keyboard_if = 0;
    usb_interface_descriptor_t first_if = {0};
    usb_interface_descriptor_t best_if = {0};

    while (off + 2 <= total_len) {
        uint8_t len  = buf[off + 0];
        uint8_t type = buf[off + 1];
        if (len == 0 || off + len > total_len) break;
        if (type == USB_DESC_TYPE_INTERFACE) {
            const usb_interface_descriptor_t *id = (const usb_interface_descriptor_t *)&buf[off];

            if (!have_any_if) {
                memory_copy(&first_if, id, sizeof(*id));
                have_any_if = 1;
            }

            // Prefer HID Boot Keyboard (class=0x03, sub=0x01, proto=0x01)
            if (id->interface_class == USB_CLASS_HID &&
                id->interface_subclass == USB_SUBCLASS_BOOT &&
                id->interface_protocol == USB_PROTOCOL_KEYBOARD) {
                memory_copy(&best_if, id, sizeof(*id));
                chose_keyboard_if = 1;
                // Don't break; keep scanning in case there are multiple such IFs; last one wins (fine).
            }
        }

        off += len;
    }

    if (!have_any_if) {
        printf("No interface descriptors in configuration blob\n");
        return 0;
    }

    if (!chose_keyboard_if) {
        // Fallback to the first interface if no keyboard present (useful for debugging other devices)
        memory_copy(&best_if, &first_if, sizeof(best_if));
    }

    memory_copy(&dev->interface_descriptor, &best_if, sizeof(best_if));

    // ---------- Pass 2: find endpoint(s) for that chosen interface ----------
    // We’ll walk again and, after seeing the chosen IF, capture the first Interrupt IN endpoint.
    off = 0;
    int in_chosen_if_block = 0;
    int have_ep_in = 0;

    while (off + 2 <= total_len) {
        uint8_t len  = buf[off + 0];
        uint8_t type = buf[off + 1];
        if (len == 0 || off + len > total_len) break;

        if (type == USB_DESC_TYPE_INTERFACE) {
            const usb_interface_descriptor_t *id = (const usb_interface_descriptor_t *)&buf[off];
            // Enter this interface “block” if it matches the chosen interface_number and alt setting.
            in_chosen_if_block = (id->interface_number == dev->interface_descriptor.interface_number) &&
                                 (id->alternate_setting == dev->interface_descriptor.alternate_setting);
        } else if (type == USB_DESC_TYPE_ENDPOINT && in_chosen_if_block) {
            const usb_endpoint_descriptor_t *ep = (const usb_endpoint_descriptor_t *)&buf[off];
            // We want Interrupt IN (type==3, dir==IN)
            if (((ep->attributes & 0x3) == 0x3) && (ep->endpoint_address & 0x80)) {
                memory_copy(&dev->endpoint_descriptors[0], ep, sizeof(*ep));
                have_ep_in = 1;
                // We can stop once we have the first interrupt IN endpoint.
                break;
            }
        }

        off += len;
    }

    if (!have_ep_in) {
        printf("No interrupt IN endpoint found for interface %u (alt %u)\n",
               dev->interface_descriptor.interface_number,
               dev->interface_descriptor.alternate_setting);
        return 0;
    }

    return 1;
}


void uhci_enumerate_device(uint16_t io_base, int port) {
    if (usb_device_count >= MAX_USB_DEVICES) {
        printf("Max USB devices reached. Cannot add device on port %d\n", port);
        return;
    }

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

    sleep_ms(10);

    usb_device_t *new_device = &usb_devices[usb_device_count++];
    new_device->address = device_address; // Add device to usb_devices array
    // Step 3: Get device descriptor
    if (!uhci_get_device_descriptor(io_base, device_address, &(new_device->descriptor))) {
        printf("Failed to get device descriptor on port %d\n", port);
        return;
    }

    // Step 4: Read only 9 bytes of configuration descriptor (to get total length)
    usb_configuration_descriptor_t short_config;
    if (!uhci_get_configuration_descriptor(io_base, device_address, &short_config)) {
        printf("    Failed to get short configuration descriptor on port %d\n", port);
        return;
    }

    uint16_t total_length = short_config.total_length;
    printf("Config total length = %u bytes\n", total_length);

    uint8_t *full_config = (uint8_t *)aligned_alloc(16, total_length);
    if (!full_config) {
        printf("Memory allocation failed for full configuration descriptor\n");
        return;
    }

    if (!uhci_get_full_configuration_descriptor(io_base, device_address, full_config, total_length)) {
        // you'll implement this like uhci_get_configuration_descriptor, but wLength = total_length
        printf("Failed to get full configuration descriptor\n");
        aligned_free(full_config);
        return;
    }

    //usb_dump_configuration_blob(full_config, total_length);
    
    
    // Parse the blob into the device record (config, interface, endpoint[0])
    if (!usb_parse_config_blob_into_device(full_config, total_length, new_device)) {
        printf("Parsing configuration blob failed (no suitable interface/endpoint)\n");
        aligned_free(full_config);
        return;
    }
    

    // Step 4: Set configuration
    if (!uhci_set_configuration(io_base, device_address, 1)) {
        printf("Failed to set configuration on port %d\n", port);
        return;
    }



    const usb_interface_descriptor_t *ifs = &new_device->interface_descriptor;
    const usb_endpoint_descriptor_t  *ep  = &new_device->endpoint_descriptors[0];

    int is_hid_kbd =    (ifs->interface_class == USB_CLASS_HID) &&
                        (ifs->interface_subclass == USB_SUBCLASS_BOOT) &&
                        (ifs->interface_protocol == USB_PROTOCOL_KEYBOARD);

    printf("\n=== USB device on port %d ===\n", port);
    printf("  Address            : %u\n", (unsigned int)new_device->address);
    printf("  VID:PID            : %x:%x\n",
            (unsigned int)new_device->descriptor.vendor_id,
            (unsigned int)new_device->descriptor.product_id);
    printf("  USB spec           : 0x%x\n", (unsigned int)new_device->descriptor.usb_version);
    printf("  EP0 max packet     : %u\n", (unsigned int)new_device->descriptor.max_packet_size);

    printf("  Config value       : %u\n", (unsigned int)new_device->config_descriptor.configuration_value);
    printf("  Interfaces         : %u\n", (unsigned int)new_device->config_descriptor.num_interfaces);
    printf("  Attributes         : 0x%x\n", (unsigned int)new_device->config_descriptor.attributes);
    printf("  Max power (2mA)    : %u\n", (unsigned int)new_device->config_descriptor.max_power);

    printf("  Chosen IF          : num=%u alt=%u class=0x%x subcls=0x%x proto=0x%x\n",
            (unsigned int)ifs->interface_number,
            (unsigned int)ifs->alternate_setting,
            (unsigned int)ifs->interface_class,
            (unsigned int)ifs->interface_subclass,
            (unsigned int)ifs->interface_protocol);
    printf("  HID Boot Keyboard  : %s\n", is_hid_kbd ? "yes" : "no");

    // Endpoint 0 holds the first INT IN we parsed for this IF
    printf("  INT IN endpoint    : addr=0x%x  (ep=%u, %s)\n",
            (unsigned int)ep->endpoint_address,
            (unsigned int)(ep->endpoint_address & 0x0F),
            (ep->endpoint_address & 0x80) ? "IN" : "OUT");
    printf("    type(attr)       : 0x%x  (expect 0x3 for interrupt)\n",
            (unsigned int)ep->attributes);
    printf("    wMaxPacketSize   : %u\n", (unsigned int)ep->max_packet_size);
    printf("    bInterval (ms)   : %u\n", (unsigned int)ep->interval);

    // Handy hint for your scheduler:
    if (is_hid_kbd) {
        printf("  -> Poll this endpoint every %u ms (frame interval), keep a persistent TD in the frame list.\n",
                (unsigned int)ep->interval);
    }
    printf("===============================\n\n");

    //sleep_ms(1000);
}


void uhci_enumerate_devices(usb_controller_t* controller) {
    uint16_t io_base = (uint16_t)(controller->base_address);
    for (int port = 0; port < NUM_PORTS; port++) {
        uhci_enumerate_device(io_base, port);
    }
}


