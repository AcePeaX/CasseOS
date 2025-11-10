#include "cpu/ports.h"
#include "cpu/timer.h"
#include "libc/mem.h"
#include "libc/function.h"
#include <stddef.h>

#include "uhci.h"
#include "../pci.h"

uint32_t find_uhci_io_base(pci_device_t *device)
{
    for (int i = 0; i < 6; i++)
    { // Check all 6 BARs
        uint32_t bar = device->bar[i];
        if (bar != 0 && !device->is_memory_mapped[i])
        {                                        // I/O space BAR
            return (uint16_t)(bar & 0xFFFFFFFC); // Return the base address
        }
    }
    return 0; // No valid BAR found
}

bool uhci_reset_controller(uint16_t io_base)
{
    // Global Reset (bit 2) in Command (0x00)
    port_word_out(io_base + 0x00, 0x0002);
    sleep_ms(10);
    port_word_out(io_base + 0x00, 0x0000);

    // Check USB Status Register
    uint16_t status = port_word_in(io_base + 0x02);
    if (status & (1 << 5))
    {
        // Controller halted bit set
        return true;
    }
    else
    {
        UHCI_ERR("Controller is NOT halted. Status: 0x%x\n", status);
        return false;
    }
}

uint32_t frame_list[1024] __attribute__((aligned(4096)));

bool uhci_set_frame_list_base_address(uint16_t io_base, uint32_t frame_list_phys_addr)
{
    // Frame List Base Address Register (0x08)
    port_dword_out(io_base + 0x08, frame_list_phys_addr);
    uint32_t frame_list_address = port_dword_in(io_base + 0x08);

    if (frame_list_address == (uintptr_t)frame_list_phys_addr)
    {
        UHCI_DBG("Frame List set: FLBA=0x%x\n", frame_list_address);
        return true;
    }
    else
    {
        UHCI_ERR("Frame List set failed. Expected: 0x%x, Got: 0x%x\n",
                 (uintptr_t)frame_list_phys_addr, frame_list_address);
    }
    return false;
}

void uhci_initialize_frame_list()
{
    for (int i = 0; i < 1024; i++)
    {
        frame_list[i] = 0x00000001; // Terminate entries
    }
    UHCI_DBG("Initialized frame list with 1024 T-terminated entries\n");
}

bool uhci_start_controller(uint16_t io_base)
{
    // Run/Stop bit (bit 0) in Command Register
    port_word_out(io_base + 0x00, 0x0001);
    uint16_t status = port_word_in(io_base + 0x02); // Status Register
    if (status != 0)
    {
        UHCI_WARN("Controller start reported non-zero status: 0x%x\n", status);
        // Not necessarily fatal; UHCI sets status bits for events.
    }
    return true;
}

bool uhci_enable_interrupts(uint16_t io_base)
{
    // Interrupt Enable Register (0x04): HSE | IOC (0x0006)
    port_word_out(io_base + 0x04, 0x0006);
    uint32_t intr = port_word_in(io_base + 0x04);
    if (intr != 0x06)
    {
        UHCI_WARN("Interrupt mask mismatch: wrote 0x0006, read 0x%x\n", intr);
        return false;
    }
    UHCI_DBG("Interrupts enabled (HSE | IOC)\n");
    return true;
}

bool uhci_initialize_controller(usb_controller_t *controller)
{
    uint16_t io_base = (uint16_t)controller->base_address;

    pci_enable_bus_mastering(controller->pci_device);
    UHCI_DBG("Bus mastering enabled\n");

    if (!uhci_reset_controller(io_base))
    {
        return false;
    }

    uhci_initialize_frame_list();

    uintptr_t frame_list_phys_addr = (uintptr_t)&frame_list; // identity-mapped assumption
    if (!uhci_set_frame_list_base_address(io_base, frame_list_phys_addr))
    {
        return false;
    }

    if (!uhci_enable_interrupts(io_base))
    {
        // Not fatal for polling-only bring-up; treat as warn in early stages.
        UHCI_WARN("Interrupt enable failed; continuing with polling\n");
    }

    if (!uhci_start_controller(io_base))
    {
        UHCI_ERR("Failed to start UHCI controller\n");
        return false;
    }

    UHCI_INFO("UHCI Controller initialized at IO base 0x%x\n", io_base);
    return true;
}

void uhci_reset_port(uint16_t io_base, int port)
{
    uint16_t port_addr = io_base + PORT_SC_OFFSET + (port * 2); // each port uses 2 bytes

    // Reset
    port_word_out(port_addr, PORT_RESET);
    sleep_ms(50);
    port_word_out(port_addr, port_word_in(port_addr) & ~PORT_RESET);
    sleep_ms(50);

    // Enable the port
    uint16_t status = port_word_in(port_addr);
    port_word_out(port_addr, status | PORT_ENABLE);
    sleep_ms(10);

    // Read port status
    status = port_word_in(port_addr);
    if ((status & PORT_ENABLE) && (status & PORT_CONNECT_STATUS))
    {
        UHCI_INFO("Device connected on port %d\n", port);
    }
    else
    {
        UHCI_INFO("No device connected on port %d\n", port);
    }
}

/*
void uhci_reset_and_detect_devices(uint16_t io_base)
{
    for (int port = 0; port < NUM_PORTS; port++)
    {
        uhci_reset_port(io_base, port);
    }
}
*/

uhci_td_t *allocate_td()
{
    uhci_td_t *td = (uhci_td_t *)aligned_alloc(16, sizeof(uhci_td_t));
    if (td == NULL)
    {
        UHCI_ERR("TD allocation failed\n");
        return NULL;
    }
    memory_set(td, 0, sizeof(uhci_td_t));
    return td;
}

uhci_qh_t *allocate_qh()
{
    uhci_qh_t *qh = (uhci_qh_t *)aligned_alloc(16, sizeof(uhci_qh_t));
    if (qh == NULL)
    {
        UHCI_ERR("QH allocation failed\n");
        return NULL;
    }
    memory_set(qh, 0, sizeof(uhci_qh_t));
    return qh;
}

usb_setup_packet_t *allocate_setup_packet()
{
    usb_setup_packet_t *packet = (usb_setup_packet_t *)aligned_alloc(16, sizeof(usb_setup_packet_t));
    if (packet == NULL)
    {
        UHCI_ERR("Failed to allocate Setup Packet\n");
        return NULL;
    }
    memory_set(packet, 0, sizeof(usb_setup_packet_t));
    return packet;
}

void free_td(uhci_td_t *td) { aligned_free(td); }
void free_qh(uhci_qh_t *qh) { aligned_free(qh); }
void free_setup_packet(usb_setup_packet_t *setup_packet) { aligned_free(setup_packet); }

int uhci_wait_for_transfer_complete(uhci_td_t *td)
{
    int timeout = 3000; // ms
    UHCI_TRACE("Waiting for TD completion @%p\n", td);
    while ((td->control_status & 0x800000) && timeout > 0)
    {
        sleep_ms(1);
        timeout--;
    }
    if (timeout == 0)
    {
        UHCI_ERR("Transfer timeout (TD status=0x%x)\n", td->control_status);
        return -1;
    }
    if (td->control_status & (1 << 22))
    {
        UHCI_ERR("Transfer stalled\n");
        return -2;
    }
    if (td->control_status & (1 << 21))
    {
        UHCI_ERR("Data Buffer Error\n");
        return -3;
    }
    if (td->control_status & (1 << 20))
    {
        UHCI_ERR("Babble Detected\n");
        return -4;
    }
    if (td->control_status & (1 << 19))
    {
        UHCI_WARN("NAK Received\n");
        return -5;
    }
    if (td->control_status & (1 << 18))
    {
        UHCI_ERR("CRC/Timeout Error\n");
        return -6;
    }
    if (td->control_status & (1 << 17))
    {
        UHCI_ERR("Bit Stuff Error\n");
        return -7;
    }
    return 1;
}

int uhci_set_device_address(uint16_t io_base, uint8_t port, uint8_t new_address)
{
    usb_setup_packet_t *setup_packet = allocate_setup_packet();
    if (setup_packet == NULL)
        return 0;

    setup_packet->bmRequestType = 0x00; // Host->Device, Standard, Device
    setup_packet->bRequest = 0x05;      // SET_ADDRESS
    setup_packet->wValue = new_address;
    setup_packet->wIndex = 0;
    setup_packet->wLength = 0;

    UNUSED(port);

    uhci_td_t *setup_td = allocate_td();
    uhci_td_t *status_td = allocate_td();
    uhci_qh_t *qh = allocate_qh();
    if (!setup_td || !status_td || !qh)
    {
        UHCI_ERR("Error allocating TDs/QH for SET_ADDRESS\n");
        return 0;
    }

    setup_td->link_pointer = get_physical_address(status_td) | 0x04;
    setup_td->control_status = 0x800000;                         // Active
    setup_td->token = (0x2D) | (0 << 8) | (0 << 15) | (7 << 21); // PID_SETUP, addr0, ep0, len8
    setup_td->buffer_pointer = get_physical_address(setup_packet);

    status_td->link_pointer = 0x00000001;                         // Terminate
    status_td->control_status = 0x800000;                         // Active
    status_td->token = (0x69) | (0 << 8) | (0 << 15) | (0 << 21); // PID_IN, addr0, ep0, len0
    status_td->buffer_pointer = 0;

    qh->horizontal_link_pointer = 0x00000001;
    qh->vertical_link_pointer = get_physical_address(setup_td);

    uint16_t current_frame = port_word_in(io_base + 0x06) & 0x7FF;
    uint16_t frame_number = current_frame + 1;
    frame_list[(frame_number) % 1024] = get_physical_address(qh) | 0x00000002;
    frame_list[(frame_number + 1) % 1024] = get_physical_address(qh) | 0x00000002;

    uint16_t command = port_word_in(io_base + 0x00);
    if (!(command & 0x01))
    {
        UHCI_ERR("Controller is not running\n");
        return 0;
    }

    if (uhci_wait_for_transfer_complete(status_td) != 1)
    {
        UHCI_ERR("SET_ADDRESS completion failed\n");
        return 0;
    }

    uint16_t status = port_word_in(io_base + 0x02);
    if (status & 0x02)
    {
        UHCI_WARN("USB Error Interrupt occurred during SET_ADDRESS, clearing\n");
        port_word_out(io_base + 0x02, 0x02);
    }

    if (setup_td->control_status & 0x400000)
    {
        UHCI_ERR("Error in SET_ADDRESS transfer\n");
        UHCI_DBG(" TD Setup Status: 0x%x\n", setup_td->control_status);
        UHCI_DBG(" TD Status      : 0x%x\n", status_td->control_status);
        frame_list[frame_number % 1024] = 0x00000001;
        frame_list[(frame_number + 1) % 1024] = 0x00000001;
        free_td(setup_td);
        free_td(status_td);
        free_qh(qh);
        free_setup_packet(setup_packet);
        return 0;
    }

    frame_list[frame_number % 1024] = 0x00000001;
    frame_list[(frame_number + 1) % 1024] = 0x00000001;
    free_td(setup_td);
    free_td(status_td);
    free_qh(qh);
    free_setup_packet(setup_packet);

    sleep_ms(10); // device applies new address
    UHCI_INFO("SET_ADDRESS -> %u OK\n", new_address);
    return 1;
}

int uhci_get_device_descriptor(uint16_t io_base, uint8_t device_address, usb_device_descriptor_t *device_desc)
{
    usb_setup_packet_t *setup_packet = allocate_setup_packet();
    if (setup_packet == NULL)
        return 0;

    setup_packet->bmRequestType = 0x80; // Device->Host
    setup_packet->bRequest = 0x06;      // GET_DESCRIPTOR
    setup_packet->wValue = (USB_DESC_TYPE_DEVICE << 8) | 0x00;
    setup_packet->wIndex = 0;
    setup_packet->wLength = sizeof(usb_device_descriptor_t);

    uhci_td_t *setup_td = allocate_td();
    uhci_td_t *data_td = allocate_td();
    uhci_td_t *status_td = allocate_td();
    uhci_qh_t *qh = allocate_qh();

    if (!setup_td || !data_td || !status_td || !qh)
    {
        UHCI_ERR("Error allocating TDs/QH for GET_DEVICE_DESCRIPTOR\n");
        return 0;
    }

    setup_td->link_pointer = get_physical_address(data_td) | 0x04;
    setup_td->control_status = 0x800000;
    setup_td->token = (0x2D) | (device_address << 8) | (0 << 15) | (7 << 21);
    setup_td->buffer_pointer = get_physical_address(setup_packet);

    data_td->link_pointer = get_physical_address(status_td) | 0x04;
    data_td->control_status = 0x800000;
    data_td->token = (0x69) | (device_address << 8) | (1 << 19) | ((sizeof(usb_device_descriptor_t) - 1) << 21);
    data_td->buffer_pointer = get_physical_address(device_desc);

    status_td->link_pointer = 0x00000001;
    status_td->control_status = 0x800000;
    status_td->token = (0xE1) | (device_address << 8) | (1 << 19) | (0 << 21);
    status_td->buffer_pointer = 0;

    qh->horizontal_link_pointer = 0x00000001;
    qh->vertical_link_pointer = get_physical_address(setup_td);

    uint16_t frame_number = (port_word_in(io_base + 0x06) & 0x7FF) + 1;
    frame_list[(frame_number) % 1024] = get_physical_address(qh) | 0x00000002;
    frame_list[(frame_number + 1) % 1024] = get_physical_address(qh) | 0x00000002;

    if (uhci_wait_for_transfer_complete(status_td) != 1)
    {
        UHCI_ERR("GET_DEVICE_DESCRIPTOR completion failed\n");
        return 0;
    }

    if (data_td->control_status & 0x400000)
    {
        UHCI_ERR("Error in GET_DEVICE_DESCRIPTOR transfer\n");
        frame_list[(frame_number) % 1024] = 0x00000001;
        frame_list[(frame_number + 1) % 1024] = 0x00000001;
        free_td(setup_td);
        free_td(data_td);
        free_td(status_td);
        free_qh(qh);
        return 0;
    }

    frame_list[(frame_number) % 1024] = 0x00000001;
    frame_list[(frame_number + 1) % 1024] = 0x00000001;
    free_td(setup_td);
    free_td(data_td);
    free_td(status_td);
    free_qh(qh);

    UHCI_INFO("Got DEVICE descriptor: VID=0x%x PID=0x%x\n",
              device_desc->vendor_id, device_desc->product_id);
    return 1;
}

int uhci_set_configuration(uint16_t io_base, uint8_t device_address, uint8_t configuration_value)
{
    usb_setup_packet_t *setup_packet = allocate_setup_packet();
    if (setup_packet == NULL)
        return 0;

    setup_packet->bmRequestType = 0x00; // Host->Device
    setup_packet->bRequest = 0x09;      // SET_CONFIGURATION
    setup_packet->wValue = configuration_value;
    setup_packet->wIndex = 0;
    setup_packet->wLength = 0;

    uhci_td_t *setup_td = allocate_td();
    uhci_td_t *status_td = allocate_td();
    uhci_qh_t *qh = allocate_qh();

    if (!setup_td || !status_td || !qh)
    {
        UHCI_ERR("Error allocating TDs/QH for SET_CONFIGURATION\n");
        return 0;
    }

    setup_td->link_pointer = get_physical_address(status_td) | 0x04;
    setup_td->control_status = 0x800000;
    setup_td->token = (0x2D) | (device_address << 8) | (0 << 15) | (7 << 21);
    setup_td->buffer_pointer = get_physical_address(setup_packet);

    status_td->link_pointer = 0x00000001;
    status_td->control_status = 0x800000;
    status_td->token = (0x69) | (device_address << 8) | (1 << 19) | (0 << 21);
    status_td->buffer_pointer = 0;

    qh->horizontal_link_pointer = 0x00000001;
    qh->vertical_link_pointer = get_physical_address(setup_td);

    uint16_t frame_number = (port_word_in(io_base + 0x06) & 0x7FF) + 1;
    frame_list[frame_number % 1024] = get_physical_address(qh) | 0x00000002;
    frame_list[(frame_number + 1) % 1024] = get_physical_address(qh) | 0x00000002;

    if (uhci_wait_for_transfer_complete(status_td) != 1)
    {
        UHCI_ERR("SET_CONFIGURATION completion failed\n");
        return 0;
    }

    if (status_td->control_status & (1 << 22))
    {
        UHCI_ERR("SET_CONFIGURATION stalled\n");
        frame_list[frame_number % 1024] = 0x00000001;
        frame_list[(frame_number + 1) % 1024] = 0x00000001;
        free_td(setup_td);
        free_td(status_td);
        free_qh(qh);
        return 0;
    }

    frame_list[frame_number % 1024] = 0x00000001;
    frame_list[(frame_number + 1) % 1024] = 0x00000001;
    free_td(setup_td);
    free_td(status_td);
    free_qh(qh);

    UHCI_INFO("SET_CONFIGURATION -> %u OK\n", configuration_value);
    return 1;
}

int uhci_get_configuration_descriptor(uint16_t io_base, uint8_t device_address, usb_configuration_descriptor_t *config_desc)
{
    usb_setup_packet_t *setup_packet = allocate_setup_packet();
    if (setup_packet == NULL)
        return 0;

    setup_packet->bmRequestType = 0x80; // Device->Host
    setup_packet->bRequest = 0x06;      // GET_DESCRIPTOR
    setup_packet->wValue = (USB_DESC_TYPE_CONFIGURATION << 8) | 0x00;
    setup_packet->wIndex = 0;
    setup_packet->wLength = sizeof(usb_configuration_descriptor_t);

    uhci_td_t *setup_td = allocate_td();
    uhci_td_t *data_td = allocate_td();
    uhci_td_t *status_td = allocate_td();
    uhci_qh_t *qh = allocate_qh();

    if (!setup_td || !data_td || !status_td || !qh)
    {
        UHCI_ERR("Error allocating TDs/QH for GET_CONFIG_DESCRIPTOR\n");
        return 0;
    }

    setup_td->link_pointer = get_physical_address(data_td) | 0x04;
    setup_td->control_status = 0x800000;
    setup_td->token = (0x2D) | (device_address << 8) | (0 << 15) | (7 << 21);
    setup_td->buffer_pointer = get_physical_address(setup_packet);

    data_td->link_pointer = get_physical_address(status_td) | 0x04;
    data_td->control_status = 0x800000;
    data_td->token = (0x69) | (device_address << 8) | (1 << 19) | ((sizeof(usb_configuration_descriptor_t) - 1) << 21);
    data_td->buffer_pointer = get_physical_address(config_desc);

    status_td->link_pointer = 0x00000001;
    status_td->control_status = 0x800000;
    status_td->token = (0xE1) | (device_address << 8) | (1 << 19) | (0 << 21);
    status_td->buffer_pointer = 0;

    qh->horizontal_link_pointer = 0x00000001;
    qh->vertical_link_pointer = get_physical_address(setup_td);

    uint16_t frame_number = port_word_in(io_base + 0x06) & 0x7FF;
    frame_list[frame_number % 1024] = get_physical_address(qh) | 0x00000002;

    if (uhci_wait_for_transfer_complete(data_td) != 1)
    {
        UHCI_ERR("GET_CONFIG_DESCRIPTOR completion failed\n");
        free_td(setup_td);
        free_td(data_td);
        free_td(status_td);
        free_qh(qh);
        return 0;
    }

    frame_list[frame_number % 1024] = 0x00000001;
    free_td(setup_td);
    free_td(data_td);
    free_td(status_td);
    free_qh(qh);

    UHCI_DBG("Short CONFIG descriptor: total_len=%u ifaces=%u\n",
             config_desc->total_length, config_desc->num_interfaces);
    return 1;
}

int uhci_get_full_configuration_descriptor(uint16_t io_base, uint8_t device_address,
                                           uint8_t *buffer, uint16_t total_length)
{
    usb_setup_packet_t *setup_packet = allocate_setup_packet();
    if (!setup_packet)
        return 0;

    setup_packet->bmRequestType = 0x80; // Device->Host
    setup_packet->bRequest = 0x06;      // GET_DESCRIPTOR
    setup_packet->wValue = (USB_DESC_TYPE_CONFIGURATION << 8) | 0x00;
    setup_packet->wIndex = 0;
    setup_packet->wLength = total_length;

    uhci_td_t *setup_td = allocate_td();
    uhci_td_t *data_td = allocate_td();
    uhci_td_t *status_td = allocate_td();
    uhci_qh_t *qh = allocate_qh();

    if (!setup_td || !data_td || !status_td || !qh)
    {
        UHCI_ERR("Allocation failed for full config descriptor transfer\n");
        return 0;
    }

    setup_td->link_pointer = get_physical_address(data_td) | 0x04;
    setup_td->control_status = 0x800000;
    setup_td->token = (0x2D) | (device_address << 8) | (0 << 15) | (7 << 21);
    setup_td->buffer_pointer = get_physical_address(setup_packet);

    data_td->link_pointer = get_physical_address(status_td) | 0x04;
    data_td->control_status = 0x800000;
    data_td->token = (0x69) | (device_address << 8) | (1 << 19) | ((total_length - 1) << 21);
    data_td->buffer_pointer = get_physical_address(buffer);

    status_td->link_pointer = 0x00000001;
    status_td->control_status = 0x800000;
    status_td->token = (0xE1) | (device_address << 8) | (1 << 19) | (0 << 21);
    status_td->buffer_pointer = 0;

    qh->horizontal_link_pointer = 0x00000001;
    qh->vertical_link_pointer = get_physical_address(setup_td);

    uint16_t frame_number = (port_word_in(io_base + 0x06) & 0x7FF);
    frame_list[frame_number % 1024] = get_physical_address(qh) | 0x00000002;

    if (uhci_wait_for_transfer_complete(data_td) != 1)
    {
        UHCI_ERR("GET_DESCRIPTOR (full config) failed\n");
        free_td(setup_td);
        free_td(data_td);
        free_td(status_td);
        free_qh(qh);
        free_setup_packet(setup_packet);
        return 0;
    }

    frame_list[frame_number % 1024] = 0x00000001;
    free_td(setup_td);
    free_td(data_td);
    free_td(status_td);
    free_qh(qh);
    free_setup_packet(setup_packet);

    UHCI_DBG("Full CONFIG blob fetched: %u bytes\n", total_length);
    return 1;
}

#ifdef UHCI_ENABLE_DUMP
// Pretty-print the full Configuration descriptor blob (Config + Interface + HID + Endpoint)
static void usb_dump_configuration_blob(const uint8_t *buf, uint16_t total_len)
{
    uint16_t off = 0;

    UHCI_DBG("----- USB Configuration Blob (%u bytes) -----\n", (unsigned int)total_len);

    while (off + 2 <= total_len)
    {
        uint8_t bLength = buf[off];
        uint8_t bType = buf[off + 1];

        if (bLength == 0)
        {
            UHCI_WARN("  [!] bLength == 0 at offset %u, stopping.\n", (unsigned int)off);
            break;
        }
        if (off + bLength > total_len)
        {
            UHCI_WARN("  [!] Descriptor overruns buffer: off=%u len=%u total=%u\n",
                      (unsigned int)off, (unsigned int)bLength, (unsigned int)total_len);
            break;
        }

        switch (bType)
        {
        case USB_DESC_TYPE_CONFIGURATION:
        {
            const usb_configuration_descriptor_t *cd =
                (const usb_configuration_descriptor_t *)&buf[off];
            UHCI_DBG("\nCONFIGURATION @%u\n", (unsigned int)off);
            UHCI_DBG("  total_length=%u  num_interfaces=%u\n",
                     (unsigned int)cd->total_length, cd->num_interfaces);
            UHCI_DBG("  value=%u  index=%u  attrs=0x%x  max_power=%u\n",
                     cd->configuration_value, cd->configuration_index,
                     cd->attributes, cd->max_power);
            break;
        }
        case USB_DESC_TYPE_INTERFACE:
        {
            const usb_interface_descriptor_t *id =
                (const usb_interface_descriptor_t *)&buf[off];
            UHCI_DBG("\nINTERFACE @%u\n", (unsigned int)off);
            UHCI_DBG("  num=%u  alt=%u  eps=%u\n",
                     id->interface_number, id->alternate_setting, id->num_endpoints);
            UHCI_DBG("  class=0x%x  subcls=0x%x  proto=0x%x  idx=%u\n",
                     id->interface_class, id->interface_subclass,
                     id->interface_protocol, id->interface_index);
            break;
        }
        case USB_DESC_TYPE_HID:
        {
            const usb_hid_descriptor_t *hid =
                (const usb_hid_descriptor_t *)&buf[off];
            UHCI_DBG("\nHID @%u\n", (unsigned int)off);
            UHCI_DBG("  ver=0x%x  country=%u  descs=%u\n",
                     hid->hid_version, hid->country_code, hid->num_descriptors);
            UHCI_DBG("  report_type=0x%x  report_len=%u\n",
                     hid->report_type, hid->report_length);
            break;
        }
        case USB_DESC_TYPE_ENDPOINT:
        {
            const usb_endpoint_descriptor_t *ep =
                (const usb_endpoint_descriptor_t *)&buf[off];
            UHCI_DBG("\nENDPOINT @%u\n", (unsigned int)off);
            uint8_t dir = (ep->endpoint_address & 0x80) ? 1 : 0;
            UHCI_DBG("  addr=0x%x  dir=%s  attr=0x%x\n",
                     ep->endpoint_address, dir ? "IN" : "OUT", ep->attributes);
            UHCI_DBG("  max_packet=%u  interval=%u\n",
                     ep->max_packet_size, ep->interval);
            break;
        }
        default:
            UHCI_DBG("\nUNKNOWN DESC type=0x%x len=%u @%u\n",
                     bType, bLength, (unsigned int)off);
            break;
        }

        off += bLength;
    }

    UHCI_DBG("----- End of Configuration Blob -----\n");
}
#endif // UHCI_ENABLE_DUMP

// Robust two-pass parser.
static int usb_parse_config_blob_into_device(const uint8_t *buf, uint16_t total_len, usb_device_t *dev)
{
    if (!buf || !dev)
        return 0;
    if (total_len < sizeof(usb_configuration_descriptor_t))
        return 0;

    memory_copy(&dev->config_descriptor, buf, sizeof(usb_configuration_descriptor_t));

    // Pass 1: choose interface
    uint16_t off = 0;
    int have_any_if = 0;
    int chose_keyboard_if = 0;
    usb_interface_descriptor_t first_if = {0};
    usb_interface_descriptor_t best_if = {0};

    while (off + 2 <= total_len)
    {
        uint8_t len = buf[off + 0];
        uint8_t type = buf[off + 1];
        if (len == 0 || off + len > total_len)
            break;
        if (type == USB_DESC_TYPE_INTERFACE)
        {
            const usb_interface_descriptor_t *id = (const usb_interface_descriptor_t *)&buf[off];

            if (!have_any_if)
            {
                memory_copy(&first_if, id, sizeof(*id));
                have_any_if = 1;
            }

            if (id->interface_class == USB_CLASS_HID &&
                id->interface_subclass == USB_SUBCLASS_BOOT &&
                id->interface_protocol == USB_PROTOCOL_KEYBOARD)
            {
                memory_copy(&best_if, id, sizeof(*id));
                chose_keyboard_if = 1;
            }
        }

        off += len;
    }

    if (!have_any_if)
    {
        UHCI_ERR("No interface descriptors in configuration blob\n");
        return 0;
    }

    if (!chose_keyboard_if)
    {
        memory_copy(&best_if, &first_if, sizeof(best_if));
        UHCI_DBG("No HID Boot Keyboard IF; falling back to first interface\n");
    }

    memory_copy(&dev->interface_descriptor, &best_if, sizeof(best_if));

    // Pass 2: find first Interrupt IN endpoint in chosen IF
    off = 0;
    int in_chosen_if_block = 0;
    int have_ep_in = 0;

    while (off + 2 <= total_len)
    {
        uint8_t len = buf[off + 0];
        uint8_t type = buf[off + 1];
        if (len == 0 || off + len > total_len)
            break;

        if (type == USB_DESC_TYPE_INTERFACE)
        {
            const usb_interface_descriptor_t *id = (const usb_interface_descriptor_t *)&buf[off];
            in_chosen_if_block = (id->interface_number == dev->interface_descriptor.interface_number) &&
                                 (id->alternate_setting == dev->interface_descriptor.alternate_setting);
        }
        else if (type == USB_DESC_TYPE_ENDPOINT && in_chosen_if_block)
        {
            const usb_endpoint_descriptor_t *ep = (const usb_endpoint_descriptor_t *)&buf[off];
            if (((ep->attributes & 0x3) == 0x3) && (ep->endpoint_address & 0x80))
            {
                memory_copy(&dev->endpoint_descriptors[0], ep, sizeof(*ep));
                have_ep_in = 1;
                break;
            }
        }

        off += len;
    }

    if (!have_ep_in)
    {
        UHCI_ERR("No interrupt IN endpoint found for interface %u (alt %u)\n",
                 dev->interface_descriptor.interface_number,
                 dev->interface_descriptor.alternate_setting);
        return 0;
    }

    return 1;
}

void uhci_enumerate_device(uint16_t io_base, int port)
{
    if (usb_device_count >= MAX_USB_DEVICES)
    {
        UHCI_WARN("Max USB devices reached. Cannot add device on port %d\n", port);
        return;
    }

    uint8_t device_address = port + 1; // unique per port

    // Step 1: Reset and detect device
    uhci_reset_port(io_base, port);
    uint16_t port_status = port_word_in(io_base + PORT_SC_OFFSET + (port * 2));
    if (!((port_status & PORT_ENABLE) && (port_status & PORT_CONNECT_STATUS)))
    {
        UHCI_INFO("No device to enumerate on port %d\n", port);
        return;
    }

    // Step 2: Set device address
    if (!uhci_set_device_address(io_base, port, device_address))
    {
        UHCI_ERR("Failed to set device address on port %d\n", port);
        return;
    }

    sleep_ms(10);

    usb_device_t *new_device = &usb_devices[usb_device_count++];
    new_device->address = device_address;

    // Step 3: Get device descriptor
    if (!uhci_get_device_descriptor(io_base, device_address, &(new_device->descriptor)))
    {
        UHCI_ERR("Failed to get device descriptor on port %d\n", port);
        return;
    }

    // Step 4: Read 9 bytes of configuration descriptor (total length)
    usb_configuration_descriptor_t short_config;
    if (!uhci_get_configuration_descriptor(io_base, device_address, &short_config))
    {
        UHCI_ERR("Failed to get short configuration descriptor on port %d\n", port);
        return;
    }

    uint16_t total_length = short_config.total_length;
    UHCI_DBG("Config total length = %u bytes\n", total_length);

    uint8_t *full_config = (uint8_t *)aligned_alloc(16, total_length);
    if (!full_config)
    {
        UHCI_ERR("Memory allocation failed for full configuration descriptor\n");
        return;
    }

    if (!uhci_get_full_configuration_descriptor(io_base, device_address, full_config, total_length))
    {
        UHCI_ERR("Failed to get full configuration descriptor\n");
        aligned_free(full_config);
        return;
    }

#ifdef UHCI_ENABLE_DUMP
    usb_dump_configuration_blob(full_config, total_length);
#endif

    // Parse the blob into the device record (config, interface, endpoint[0])
    if (!usb_parse_config_blob_into_device(full_config, total_length, new_device))
    {
        UHCI_WARN("Parsing configuration blob failed (no suitable interface/endpoint)\n");
        aligned_free(full_config);
        return;
    }

    aligned_free(full_config);

    // Step 5: Set configuration (use the one we parsed)
    if (!uhci_set_configuration(io_base, device_address, new_device->config_descriptor.configuration_value))
    {
        UHCI_ERR("Failed to set configuration on port %d\n", port);
        return;
    }

    const usb_interface_descriptor_t *ifs = &new_device->interface_descriptor;
    const usb_endpoint_descriptor_t *ep = &new_device->endpoint_descriptors[0];

    int is_hid_kbd = (ifs->interface_class == USB_CLASS_HID) &&
                     (ifs->interface_subclass == USB_SUBCLASS_BOOT) &&
                     (ifs->interface_protocol == USB_PROTOCOL_KEYBOARD);

    printf("\n");
    UHCI_INFO("=== USB device on port %d ===\n", port);
    UHCI_INFO("  Address            : %u\n", (unsigned int)new_device->address);
    UHCI_INFO("  VID:PID            : %x:%x\n",
              (unsigned int)new_device->descriptor.vendor_id,
              (unsigned int)new_device->descriptor.product_id);
    UHCI_INFO("  USB spec           : 0x%x\n", (unsigned int)new_device->descriptor.usb_version);
    UHCI_INFO("  EP0 max packet     : %u\n", (unsigned int)new_device->descriptor.max_packet_size);

    UHCI_INFO("  Config value       : %u\n", (unsigned int)new_device->config_descriptor.configuration_value);
    UHCI_INFO("  Interfaces         : %u\n", (unsigned int)new_device->config_descriptor.num_interfaces);
    UHCI_INFO("  Attributes         : 0x%x\n", (unsigned int)new_device->config_descriptor.attributes);
    UHCI_INFO("  Max power (2mA)    : %u\n", (unsigned int)new_device->config_descriptor.max_power);

    UHCI_INFO("  Chosen IF          : num=%u alt=%u class=0x%x subcls=0x%x proto=0x%x\n",
              (unsigned int)ifs->interface_number,
              (unsigned int)ifs->alternate_setting,
              (unsigned int)ifs->interface_class,
              (unsigned int)ifs->interface_subclass,
              (unsigned int)ifs->interface_protocol);
    UHCI_INFO("  HID Boot Keyboard  : %s\n", is_hid_kbd ? "yes" : "no");

    UHCI_INFO("  INT IN endpoint    : addr=0x%x  (ep=%u, %s)\n",
              (unsigned int)ep->endpoint_address,
              (unsigned int)(ep->endpoint_address & 0x0F),
              (ep->endpoint_address & 0x80) ? "IN" : "OUT");
    UHCI_INFO("    type(attr)       : 0x%x  (expect 0x3 for interrupt)\n",
              (unsigned int)ep->attributes);
    UHCI_INFO("    wMaxPacketSize   : %u\n", (unsigned int)ep->max_packet_size);
    UHCI_INFO("    bInterval (ms)   : %u\n", (unsigned int)ep->interval);

    if (is_hid_kbd)
    {
        UHCI_INFO("  -> Poll this endpoint every %u ms; keep a persistent TD in the frame list.\n",
                  (unsigned int)ep->interval);
    }
    UHCI_INFO("===============================\n\n");
}

void uhci_enumerate_devices(usb_controller_t *controller)
{
    uint16_t io_base = (uint16_t)(controller->base_address);
    for (int port = 0; port < NUM_PORTS; port++)
    {
        uhci_enumerate_device(io_base, port);
    }
}
