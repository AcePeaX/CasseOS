#include "uhci.h"
#include "../usb.h"
#include "cpu/ports.h"
#include "cpu/timer.h"
#include "libc/mem.h"
#include "libc/function.h"

// TD/QH/setup types come from your existing uhci headers
// allocate/free helpers kept TU-local (not exported)

static uhci_td_t *allocate_td(void)
{
    uhci_td_t *td = (uhci_td_t *)aligned_alloc(16, sizeof(uhci_td_t));
    if (!td) { UHCI_ERR("TD allocation failed\n"); return NULL; }
    memory_set(td, 0, sizeof(*td));
    return td;
}

static uhci_qh_t *allocate_qh(void)
{
    uhci_qh_t *qh = (uhci_qh_t *)aligned_alloc(16, sizeof(uhci_qh_t));
    if (!qh) { UHCI_ERR("QH allocation failed\n"); return NULL; }
    memory_set(qh, 0, sizeof(*qh));
    return qh;
}

static usb_setup_packet_t *allocate_setup_packet(void)
{
    usb_setup_packet_t *p = (usb_setup_packet_t *)aligned_alloc(16, sizeof(usb_setup_packet_t));
    if (!p) { UHCI_ERR("Failed to allocate Setup Packet\n"); return NULL; }
    memory_set(p, 0, sizeof(*p));
    return p;
}

static void free_td(uhci_td_t *td) { aligned_free(td); }
static void free_qh(uhci_qh_t *qh) { aligned_free(qh); }
static void free_setup_packet(usb_setup_packet_t *sp) { aligned_free(sp); }

static int uhci_wait_for_transfer_complete(uhci_td_t *td)
{
    int timeout = 3000; // ms
    UHCI_TRACE("Waiting for TD completion @%p\n", td);
    while ((td->control_status & 0x800000) && timeout-- > 0) sleep_ms(1);

    if (timeout <= 0)      { UHCI_ERR("Transfer timeout (TD=0x%x)\n", td->control_status); return -1; }
    if (td->control_status & (1 << 22)) { UHCI_ERR("Transfer stalled\n"); return -2; }
    if (td->control_status & (1 << 21)) { UHCI_ERR("Data Buffer Error\n"); return -3; }
    if (td->control_status & (1 << 20)) { UHCI_ERR("Babble Detected\n"); return -4; }
    if (td->control_status & (1 << 19)) { UHCI_WARN("NAK Received\n");   return -5; }
    if (td->control_status & (1 << 18)) { UHCI_ERR("CRC/Timeout Error\n"); return -6; }
    if (td->control_status & (1 << 17)) { UHCI_ERR("Bit Stuff Error\n"); return -7; }
    return 1;
}

// ---- Public control helpers ----

int uhci_set_device_address(uint16_t io_base, uint8_t port, uint8_t new_address)
{
    usb_setup_packet_t *sp = allocate_setup_packet();
    if (!sp) return 0;

    sp->bmRequestType = 0x00; // Host->Device, std, device
    sp->bRequest      = 0x05; // SET_ADDRESS
    sp->wValue        = new_address;
    sp->wIndex        = 0;
    sp->wLength       = 0;

    UNUSED(port);

    uhci_td_t *td_setup  = allocate_td();
    uhci_td_t *td_status = allocate_td();
    uhci_qh_t *qh        = allocate_qh();
    if (!td_setup || !td_status || !qh) { UHCI_ERR("Alloc TD/QH SET_ADDRESS\n"); return 0; }

    td_setup->link_pointer   = get_physical_address(td_status) | 0x04;
    td_setup->control_status = 0x800000;
    td_setup->token          = (0x2D) | (0 << 8) | (0 << 15) | (7 << 21);
    td_setup->buffer_pointer = get_physical_address(sp);

    td_status->link_pointer   = 0x00000001;
    td_status->control_status = 0x800000;
    td_status->token          = (0x69) | (0 << 8) | (0 << 15) | (0 << 21);
    td_status->buffer_pointer = 0;

    qh->horizontal_link_pointer = 0x00000001;
    qh->vertical_link_pointer    = get_physical_address(td_setup);

    uint16_t cur  = port_word_in(io_base + 0x06) & 0x7FF;
    uint16_t fr   = cur + 1;
    frame_list[fr % 1024]         = get_physical_address(qh) | 0x00000002;
    frame_list[(fr + 1) % 1024]   = get_physical_address(qh) | 0x00000002;

    if (!(port_word_in(io_base + 0x00) & 0x01)) { UHCI_ERR("Controller not running\n"); return 0; }

    if (uhci_wait_for_transfer_complete(td_status) != 1) { UHCI_ERR("SET_ADDRESS completion failed\n"); return 0; }

    uint16_t st = port_word_in(io_base + 0x02);
    if (st & 0x02) { UHCI_WARN("USBERRINT during SET_ADDRESS, clearing\n"); port_word_out(io_base + 0x02, 0x02); }

    if (td_setup->control_status & 0x400000) {
        UHCI_ERR("Error in SET_ADDRESS transfer (CS=0x%x)\n", td_setup->control_status);
        frame_list[fr % 1024]       = 0x00000001;
        frame_list[(fr + 1) % 1024] = 0x00000001;
        free_td(td_setup); free_td(td_status); free_qh(qh); free_setup_packet(sp);
        return 0;
    }

    frame_list[fr % 1024]       = 0x00000001;
    frame_list[(fr + 1) % 1024] = 0x00000001;
    free_td(td_setup); free_td(td_status); free_qh(qh); free_setup_packet(sp);

    sleep_ms(10);
    UHCI_INFO("SET_ADDRESS -> %u OK\n", new_address);
    return 1;
}

int uhci_get_device_descriptor(uint16_t io_base, uint8_t addr, usb_device_descriptor_t *dev_desc)
{
    usb_setup_packet_t *sp = allocate_setup_packet();
    if (!sp) return 0;

    sp->bmRequestType = 0x80;
    sp->bRequest      = 0x06;
    sp->wValue        = (USB_DESC_TYPE_DEVICE << 8) | 0x00;
    sp->wIndex        = 0;
    sp->wLength       = sizeof(*dev_desc);

    uhci_td_t *td_setup  = allocate_td();
    uhci_td_t *td_data   = allocate_td();
    uhci_td_t *td_status = allocate_td();
    uhci_qh_t *qh        = allocate_qh();
    if (!td_setup || !td_data || !td_status || !qh) { UHCI_ERR("Alloc TD/QH GET_DEVICE\n"); return 0; }

    td_setup->link_pointer   = get_physical_address(td_data) | 0x04;
    td_setup->control_status = 0x800000;
    td_setup->token          = (0x2D) | (addr << 8) | (0 << 15) | (7 << 21);
    td_setup->buffer_pointer = get_physical_address(sp);

    td_data->link_pointer   = get_physical_address(td_status) | 0x04;
    td_data->control_status = 0x800000;
    td_data->token          = (0x69) | (addr << 8) | (1 << 19) | ((sizeof(*dev_desc) - 1) << 21);
    td_data->buffer_pointer = get_physical_address(dev_desc);

    td_status->link_pointer   = 0x00000001;
    td_status->control_status = 0x800000;
    td_status->token          = (0xE1) | (addr << 8) | (1 << 19) | (0 << 21);
    td_status->buffer_pointer = 0;

    qh->horizontal_link_pointer = 0x00000001;
    qh->vertical_link_pointer    = get_physical_address(td_setup);

    uint16_t fr = (port_word_in(io_base + 0x06) & 0x7FF) + 1;
    frame_list[fr % 1024]         = get_physical_address(qh) | 0x00000002;
    frame_list[(fr + 1) % 1024]   = get_physical_address(qh) | 0x00000002;

    if (uhci_wait_for_transfer_complete(td_status) != 1) { UHCI_ERR("GET_DEVICE completion failed\n"); return 0; }

    if (td_data->control_status & 0x400000) {
        UHCI_ERR("Error in GET_DEVICE transfer\n");
        frame_list[fr % 1024]       = 0x00000001;
        frame_list[(fr + 1) % 1024] = 0x00000001;
        free_td(td_setup); free_td(td_data); free_td(td_status); free_qh(qh);
        return 0;
    }

    frame_list[fr % 1024]       = 0x00000001;
    frame_list[(fr + 1) % 1024] = 0x00000001;
    free_td(td_setup); free_td(td_data); free_td(td_status); free_qh(qh);

    UHCI_INFO("Got DEVICE descriptor: VID=0x%x PID=0x%x\n", dev_desc->vendor_id, dev_desc->product_id);
    return 1;
}

int uhci_get_configuration_descriptor(uint16_t io_base, uint8_t addr, usb_configuration_descriptor_t *cfg)
{
    usb_setup_packet_t *sp = allocate_setup_packet();
    if (!sp) return 0;

    sp->bmRequestType = 0x80;
    sp->bRequest      = 0x06;
    sp->wValue        = (USB_DESC_TYPE_CONFIGURATION << 8) | 0x00;
    sp->wIndex        = 0;
    sp->wLength       = sizeof(*cfg);

    uhci_td_t *td_setup  = allocate_td();
    uhci_td_t *td_data   = allocate_td();
    uhci_td_t *td_status = allocate_td();
    uhci_qh_t *qh        = allocate_qh();
    if (!td_setup || !td_data || !td_status || !qh) { UHCI_ERR("Alloc TD/QH GET_CONFIG\n"); return 0; }

    td_setup->link_pointer   = get_physical_address(td_data) | 0x04;
    td_setup->control_status = 0x800000;
    td_setup->token          = (0x2D) | (addr << 8) | (0 << 15) | (7 << 21);
    td_setup->buffer_pointer = get_physical_address(sp);

    td_data->link_pointer   = get_physical_address(td_status) | 0x04;
    td_data->control_status = 0x800000;
    td_data->token          = (0x69) | (addr << 8) | (1 << 19) | ((sizeof(*cfg) - 1) << 21);
    td_data->buffer_pointer = get_physical_address(cfg);

    td_status->link_pointer   = 0x00000001;
    td_status->control_status = 0x800000;
    td_status->token          = (0xE1) | (addr << 8) | (1 << 19) | (0 << 21);
    td_status->buffer_pointer = 0;

    qh->horizontal_link_pointer = 0x00000001;
    qh->vertical_link_pointer    = get_physical_address(td_setup);

    uint16_t fr = port_word_in(io_base + 0x06) & 0x7FF;
    frame_list[fr % 1024] = get_physical_address(qh) | 0x00000002;

    if (uhci_wait_for_transfer_complete(td_data) != 1) {
        UHCI_ERR("GET_CONFIG completion failed\n");
        free_td(td_setup); free_td(td_data); free_td(td_status); free_qh(qh);
        return 0;
    }

    frame_list[fr % 1024] = 0x00000001;
    free_td(td_setup); free_td(td_data); free_td(td_status); free_qh(qh);

    UHCI_DBG("Short CONFIG descriptor: total_len=%u ifaces=%u\n", cfg->total_length, cfg->num_interfaces);
    return 1;
}

int uhci_get_full_configuration_descriptor(uint16_t io_base, uint8_t addr, uint8_t *buf, uint16_t total_len)
{
    usb_setup_packet_t *sp = allocate_setup_packet();
    if (!sp) return 0;

    sp->bmRequestType = 0x80;
    sp->bRequest      = 0x06;
    sp->wValue        = (USB_DESC_TYPE_CONFIGURATION << 8) | 0x00;
    sp->wIndex        = 0;
    sp->wLength       = total_len;

    uhci_td_t *td_setup  = allocate_td();
    uhci_td_t *td_data   = allocate_td();
    uhci_td_t *td_status = allocate_td();
    uhci_qh_t *qh        = allocate_qh();
    if (!td_setup || !td_data || !td_status || !qh) { UHCI_ERR("Alloc TD/QH GET_CONFIG(full)\n"); return 0; }

    td_setup->link_pointer   = get_physical_address(td_data) | 0x04;
    td_setup->control_status = 0x800000;
    td_setup->token          = (0x2D) | (addr << 8) | (0 << 15) | (7 << 21);
    td_setup->buffer_pointer = get_physical_address(sp);

    td_data->link_pointer   = get_physical_address(td_status) | 0x04;
    td_data->control_status = 0x800000;
    td_data->token          = (0x69) | (addr << 8) | (1 << 19) | ((total_len - 1) << 21);
    td_data->buffer_pointer = get_physical_address(buf);

    td_status->link_pointer   = 0x00000001;
    td_status->control_status = 0x800000;
    td_status->token          = (0xE1) | (addr << 8) | (1 << 19) | (0 << 21);
    td_status->buffer_pointer = 0;

    qh->horizontal_link_pointer = 0x00000001;
    qh->vertical_link_pointer    = get_physical_address(td_setup);

    uint16_t fr = port_word_in(io_base + 0x06) & 0x7FF;
    frame_list[fr % 1024] = get_physical_address(qh) | 0x00000002;

    if (uhci_wait_for_transfer_complete(td_data) != 1) {
        UHCI_ERR("GET_DESCRIPTOR (full config) failed\n");
        free_td(td_setup); free_td(td_data); free_td(td_status); free_qh(qh); free_setup_packet(sp);
        return 0;
    }

    frame_list[fr % 1024] = 0x00000001;
    free_td(td_setup); free_td(td_data); free_td(td_status); free_qh(qh); free_setup_packet(sp);

    UHCI_DBG("Full CONFIG blob fetched: %u bytes\n", total_len);
    return 1;
}

int uhci_set_configuration(uint16_t io_base, uint8_t addr, uint8_t cfg_val)
{
    usb_setup_packet_t *sp = allocate_setup_packet();
    if (!sp) return 0;

    sp->bmRequestType = 0x00;
    sp->bRequest      = 0x09; // SET_CONFIGURATION
    sp->wValue        = cfg_val;
    sp->wIndex        = 0;
    sp->wLength       = 0;

    uhci_td_t *td_setup  = allocate_td();
    uhci_td_t *td_status = allocate_td();
    uhci_qh_t *qh        = allocate_qh();
    if (!td_setup || !td_status || !qh) { UHCI_ERR("Alloc TD/QH SET_CONFIG\n"); return 0; }

    td_setup->link_pointer   = get_physical_address(td_status) | 0x04;
    td_setup->control_status = 0x800000;
    td_setup->token          = (0x2D) | (addr << 8) | (0 << 15) | (7 << 21);
    td_setup->buffer_pointer = get_physical_address(sp);

    td_status->link_pointer   = 0x00000001;
    td_status->control_status = 0x800000;
    td_status->token          = (0x69) | (addr << 8) | (1 << 19) | (0 << 21);
    td_status->buffer_pointer = 0;

    qh->horizontal_link_pointer = 0x00000001;
    qh->vertical_link_pointer    = get_physical_address(td_setup);

    uint16_t fr = (port_word_in(io_base + 0x06) & 0x7FF) + 1;
    frame_list[fr % 1024]         = get_physical_address(qh) | 0x00000002;
    frame_list[(fr + 1) % 1024]   = get_physical_address(qh) | 0x00000002;

    if (uhci_wait_for_transfer_complete(td_status) != 1) { UHCI_ERR("SET_CONFIGURATION completion failed\n"); return 0; }

    if (td_status->control_status & (1 << 22)) {
        UHCI_ERR("SET_CONFIGURATION stalled\n");
        frame_list[fr % 1024]       = 0x00000001;
        frame_list[(fr + 1) % 1024] = 0x00000001;
        free_td(td_setup); free_td(td_status); free_qh(qh);
        return 0;
    }

    frame_list[fr % 1024]       = 0x00000001;
    frame_list[(fr + 1) % 1024] = 0x00000001;
    free_td(td_setup); free_td(td_status); free_qh(qh);

    UHCI_INFO("SET_CONFIGURATION -> %u OK\n", cfg_val);
    return 1;
}
