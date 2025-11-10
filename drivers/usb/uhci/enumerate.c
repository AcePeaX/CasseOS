#include "uhci.h"
#include "../usb.h"
#include "cpu/ports.h"
#include "cpu/timer.h"
#include "libc/mem.h"
#include "libc/function.h"

#ifdef UHCI_ENABLE_DUMP
static void usb_dump_configuration_blob(const uint8_t *buf, uint16_t total_len)
{
    uint16_t off = 0;
    UHCI_DBG("----- USB Configuration Blob (%u bytes) -----\n", (unsigned int)total_len);
    while (off + 2 <= total_len) {
        uint8_t len  = buf[off + 0];
        uint8_t type = buf[off + 1];
        if (len == 0 || off + len > total_len) break;

        switch (type) {
            case USB_DESC_TYPE_CONFIGURATION: {
                const usb_configuration_descriptor_t *cd = (const usb_configuration_descriptor_t *)&buf[off];
                UHCI_DBG("\nCONFIGURATION @%u\n", (unsigned int)off);
                UHCI_DBG("  total_length=%u  num_interfaces=%u\n", cd->total_length, cd->num_interfaces);
                UHCI_DBG("  value=%u  index=%u  attrs=0x%x  max_power=%u\n",
                         cd->configuration_value, cd->configuration_index, cd->attributes, cd->max_power);
                break;
            }
            case USB_DESC_TYPE_INTERFACE: {
                const usb_interface_descriptor_t *id = (const usb_interface_descriptor_t *)&buf[off];
                UHCI_DBG("\nINTERFACE @%u\n", (unsigned int)off);
                UHCI_DBG("  num=%u  alt=%u  eps=%u\n", id->interface_number, id->alternate_setting, id->num_endpoints);
                UHCI_DBG("  class=0x%x  subcls=0x%x  proto=0x%x  idx=%u\n",
                         id->interface_class, id->interface_subclass, id->interface_protocol, id->interface_index);
                break;
            }
            case USB_DESC_TYPE_HID: {
                const usb_hid_descriptor_t *hid = (const usb_hid_descriptor_t *)&buf[off];
                UHCI_DBG("\nHID @%u\n", (unsigned int)off);
                UHCI_DBG("  ver=0x%x  country=%u  descs=%u\n", hid->hid_version, hid->country_code, hid->num_descriptors);
                UHCI_DBG("  report_type=0x%x  report_len=%u\n", hid->report_type, hid->report_length);
                break;
            }
            case USB_DESC_TYPE_ENDPOINT: {
                const usb_endpoint_descriptor_t *ep = (const usb_endpoint_descriptor_t *)&buf[off];
                uint8_t dir = (ep->endpoint_address & 0x80) ? 1 : 0;
                UHCI_DBG("\nENDPOINT @%u\n", (unsigned int)off);
                UHCI_DBG("  addr=0x%x  dir=%s  attr=0x%x\n",
                         ep->endpoint_address, dir ? "IN" : "OUT", ep->attributes);
                UHCI_DBG("  max_packet=%u  interval=%u\n", ep->max_packet_size, ep->interval);
                break;
            }
            default:
                UHCI_DBG("\nUNKNOWN DESC type=0x%x len=%u @%u\n", type, len, (unsigned int)off);
                break;
        }
        off += len;
    }
    UHCI_DBG("----- End of Configuration Blob -----\n");
}
#endif

// Two-pass parse: pick HID boot keyboard interface if present; then first INT IN endpoint.
static int usb_parse_config_blob_into_device(const uint8_t *buf, uint16_t total_len, usb_device_t *dev)
{
    if (!buf || !dev) return 0;
    if (total_len < sizeof(usb_configuration_descriptor_t)) return 0;

    memory_copy(&dev->config_descriptor, buf, sizeof(usb_configuration_descriptor_t));

    uint16_t off = 0;
    int have_any_if = 0, chose_keyboard_if = 0;
    usb_interface_descriptor_t first_if = (usb_interface_descriptor_t){0};
    usb_interface_descriptor_t best_if  = (usb_interface_descriptor_t){0};

    // Pass 1: choose interface
    while (off + 2 <= total_len) {
        uint8_t len = buf[off + 0], type = buf[off + 1];
        if (len == 0 || off + len > total_len) break;

        if (type == USB_DESC_TYPE_INTERFACE) {
            const usb_interface_descriptor_t *id = (const usb_interface_descriptor_t *)&buf[off];
            if (!have_any_if) { memory_copy(&first_if, id, sizeof(*id)); have_any_if = 1; }
            if (id->interface_class == USB_CLASS_HID &&
                id->interface_subclass == USB_SUBCLASS_BOOT &&
                id->interface_protocol == USB_PROTOCOL_KEYBOARD) {
                memory_copy(&best_if, id, sizeof(*id));
                chose_keyboard_if = 1;
            }
        }
        off += len;
    }

    if (!have_any_if) { UHCI_ERR("No interface descriptors in configuration blob\n"); return 0; }
    if (!chose_keyboard_if) {
        memory_copy(&best_if, &first_if, sizeof(best_if));
        UHCI_DBG("No HID Boot Keyboard IF; falling back to first interface\n");
    }
    memory_copy(&dev->interface_descriptor, &best_if, sizeof(best_if));

    // Pass 2: find first Interrupt IN EP within chosen IF block
    off = 0;
    int in_block = 0, have_ep_in = 0;
    while (off + 2 <= total_len) {
        uint8_t len = buf[off + 0], type = buf[off + 1];
        if (len == 0 || off + len > total_len) break;

        if (type == USB_DESC_TYPE_INTERFACE) {
            const usb_interface_descriptor_t *id = (const usb_interface_descriptor_t *)&buf[off];
            in_block = (id->interface_number == dev->interface_descriptor.interface_number) &&
                       (id->alternate_setting == dev->interface_descriptor.alternate_setting);
        } else if (type == USB_DESC_TYPE_ENDPOINT && in_block) {
            const usb_endpoint_descriptor_t *ep = (const usb_endpoint_descriptor_t *)&buf[off];
            if (((ep->attributes & 0x3) == 0x3) && (ep->endpoint_address & 0x80)) {
                memory_copy(&dev->endpoint_descriptors[0], ep, sizeof(*ep));
                have_ep_in = 1;
                break;
            }
        }
        off += len;
    }

    if (!have_ep_in) {
        UHCI_ERR("No interrupt IN endpoint found for interface %u (alt %u)\n",
                 dev->interface_descriptor.interface_number,
                 dev->interface_descriptor.alternate_setting);
        return 0;
    }
    return 1;
}

// Reset and enable a UHCI root hub port
void uhci_reset_port(uint16_t io_base, int port)
{
    uint16_t port_addr = io_base + PORT_SC_OFFSET + (port * 2);

    // 1. Issue port reset
    port_word_out(port_addr, PORT_RESET);
    sleep_ms(50);
    port_word_out(port_addr, port_word_in(port_addr) & ~PORT_RESET);
    sleep_ms(50);

    // 2. Enable port
    uint16_t status = port_word_in(port_addr);
    port_word_out(port_addr, status | PORT_ENABLE);
    sleep_ms(10);

    // 3. Read back and log
    status = port_word_in(port_addr);
    if ((status & PORT_ENABLE) && (status & PORT_CONNECT_STATUS))
        UHCI_INFO("Device connected on port %d\n", port);
    else {
        UHCI_INFO("No device connected on port %d\n", port);
    }
}

void uhci_enumerate_device(uint16_t io_base, int port)
{
    extern usb_device_t usb_devices[MAX_USB_DEVICES];
    extern uint8_t usb_device_count;

    if (usb_device_count >= MAX_USB_DEVICES) {
        UHCI_WARN("Max USB devices reached. Cannot add device on port %d\n", port);
        return;
    }

    uint8_t address = (uint8_t)(port + 1);

    uhci_reset_port(io_base, port);
    uint16_t ps = port_word_in(io_base + PORT_SC_OFFSET + (port * 2));
    if (!((ps & PORT_ENABLE) && (ps & PORT_CONNECT_STATUS))) {
        UHCI_INFO("No device to enumerate on port %d\n", port);
        return;
    }

    if (!uhci_set_device_address(io_base, port, address)) {
        UHCI_ERR("Failed to set device address on port %d\n", port);
        return;
    }
    sleep_ms(10);

    usb_device_t *dev = &usb_devices[usb_device_count++];
    dev->address = address;

    if (!uhci_get_device_descriptor(io_base, address, &dev->descriptor)) {
        UHCI_ERR("Failed to get device descriptor on port %d\n", port);
        return;
    }

    usb_configuration_descriptor_t short_cfg;
    if (!uhci_get_configuration_descriptor(io_base, address, &short_cfg)) {
        UHCI_ERR("Failed to get short configuration descriptor on port %d\n", port);
        return;
    }

    uint16_t total_len = short_cfg.total_length;
    UHCI_DBG("Config total length = %u bytes\n", total_len);

    uint8_t *blob = (uint8_t *)aligned_alloc(16, total_len);
    if (!blob) { UHCI_ERR("Alloc full configuration blob failed\n"); return; }

    if (!uhci_get_full_configuration_descriptor(io_base, address, blob, total_len)) {
        UHCI_ERR("Failed to get full configuration descriptor\n");
        aligned_free(blob);
        return;
    }

#ifdef UHCI_ENABLE_DUMP
    usb_dump_configuration_blob(blob, total_len);
#endif

    if (!usb_parse_config_blob_into_device(blob, total_len, dev)) {
        UHCI_WARN("Parsing configuration blob failed (no suitable interface/endpoint)\n");
        aligned_free(blob);
        return;
    }
    aligned_free(blob);

    if (!uhci_set_configuration(io_base, address, dev->config_descriptor.configuration_value)) {
        UHCI_ERR("Failed to set configuration on port %d\n", port);
        return;
    }

    const usb_interface_descriptor_t *ifs = &dev->interface_descriptor;
    const usb_endpoint_descriptor_t  *ep  = &dev->endpoint_descriptors[0];

    UNUSED(ep);

    int is_hid_kbd = (ifs->interface_class == USB_CLASS_HID) &&
                     (ifs->interface_subclass == USB_SUBCLASS_BOOT) &&
                     (ifs->interface_protocol == USB_PROTOCOL_KEYBOARD);

    UHCI_INFO("\n");
    UHCI_INFO("=== USB device on port %d ===\n", port);
    UHCI_INFO("  Address            : %u\n", (unsigned)dev->address);
    UHCI_INFO("  VID:PID            : %x:%x\n", (unsigned)dev->descriptor.vendor_id, (unsigned)dev->descriptor.product_id);
    UHCI_INFO("  USB spec           : 0x%x\n", (unsigned)dev->descriptor.usb_version);
    UHCI_INFO("  EP0 max packet     : %u\n",  (unsigned)dev->descriptor.max_packet_size);
    UHCI_INFO("  Config value       : %u\n",  (unsigned)dev->config_descriptor.configuration_value);
    UHCI_INFO("  Interfaces         : %u\n",  (unsigned)dev->config_descriptor.num_interfaces);
    UHCI_INFO("  Attributes         : 0x%x\n", (unsigned)dev->config_descriptor.attributes);
    UHCI_INFO("  Max power (2mA)    : %u\n",  (unsigned)dev->config_descriptor.max_power);
    UHCI_INFO("  Chosen IF          : num=%u alt=%u class=0x%x subcls=0x%x proto=0x%x\n",
              (unsigned)ifs->interface_number, (unsigned)ifs->alternate_setting,
              (unsigned)ifs->interface_class, (unsigned)ifs->interface_subclass, (unsigned)ifs->interface_protocol);
    UHCI_INFO("  HID Boot Keyboard  : %s\n", is_hid_kbd ? "yes" : "no");
    UHCI_INFO("  INT IN endpoint    : addr=0x%x  (ep=%u, %s)\n",
              (unsigned)ep->endpoint_address, (unsigned)(ep->endpoint_address & 0x0F),
              (ep->endpoint_address & 0x80) ? "IN" : "OUT");
    UHCI_INFO("    type(attr)       : 0x%x  (expect 0x3 for interrupt)\n", (unsigned)ep->attributes);
    UHCI_INFO("    wMaxPacketSize   : %u\n", (unsigned)ep->max_packet_size);
    UHCI_INFO("    bInterval (ms)   : %u\n", (unsigned)ep->interval);
    if (is_hid_kbd) {
        UHCI_INFO("  -> Poll this endpoint every %u ms; keep a persistent TD in the frame list.\n", (unsigned)ep->interval);
    }
    UHCI_INFO("===============================\n\n");
}

void uhci_enumerate_devices(usb_controller_t *controller)
{
    uint16_t io_base = (uint16_t)(controller->base_address);
    for (int port = 0; port < NUM_PORTS; port++) uhci_enumerate_device(io_base, port);
}
