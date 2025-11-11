#ifndef UHCI_UHCI_H
#define UHCI_UHCI_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Descriptors & base USB helpers
#include "../usb.h"
#include "drivers/pci.h"
#include "../usb_descriptors.h"

#ifndef UHCI_LOG_LEVEL
#define UHCI_LOG_LEVEL USB_LOG_LEVEL_WARN
#endif

#define UHCI_LOG(level, tag, fmt, ...) \
    USB_SUBSYS_LOG(level, UHCI_LOG_LEVEL, "[UHCI]", tag, fmt, ##__VA_ARGS__)

#define UHCI_ERR(fmt, ...)   UHCI_LOG(USB_LOG_LEVEL_ERROR, "[ERR]", fmt, ##__VA_ARGS__)
#define UHCI_WARN(fmt, ...)  UHCI_LOG(USB_LOG_LEVEL_WARN,  "[WRN]", fmt, ##__VA_ARGS__)
#define UHCI_INFO(fmt, ...)  UHCI_LOG(USB_LOG_LEVEL_INFO,  "[INF]", fmt, ##__VA_ARGS__)
#define UHCI_DBG(fmt, ...)   UHCI_LOG(USB_LOG_LEVEL_DEBUG, "[DBG]", fmt, ##__VA_ARGS__)
#define UHCI_TRACE(fmt, ...) UHCI_LOG(USB_LOG_LEVEL_TRACE, "[TRC]", fmt, ##__VA_ARGS__)

// ---- Handy macros ----
#ifndef UNUSED
#define UNUSED(x) (void)(x)
#endif

// ---- UHCI I/O register offsets & bits (you said you already added them) ----
// Keep only if not already defined in your project-wide uhci header.
#ifndef PORT_SC_OFFSET
#define PORT_SC_OFFSET 0x10
#endif
#ifndef PORT_ENABLE
#define PORT_ENABLE (1 << 2)
#endif
#ifndef PORT_CONNECT_STATUS
#define PORT_CONNECT_STATUS (1 << 0)
#endif
#ifndef PORT_RESET
#define PORT_RESET (1 << 9)
#endif

#ifndef NUM_PORTS
#define NUM_PORTS 2 // Many UHCI controllers expose 2 root ports
#endif

// ---- Public API ----

// Locate UHCI I/O base from PCI BARs
uint32_t find_uhci_io_base(pci_device_t *device);

// Bring-up controller (reset, frame list, enable interrupts, run)
bool uhci_initialize_controller(usb_controller_t *controller);

// Port helper
void uhci_reset_port(uint16_t io_base, int port);

// Control transfers you already implemented (exported because enumerate uses them)
int uhci_set_device_address(uint16_t io_base, uint8_t port, uint8_t new_address);
int uhci_get_device_descriptor(uint16_t io_base, uint8_t device_address, usb_device_descriptor_t *device_desc);
int uhci_get_configuration_descriptor(uint16_t io_base, uint8_t device_address, usb_configuration_descriptor_t *config_desc);
int uhci_get_full_configuration_descriptor(uint16_t io_base, uint8_t device_address, uint8_t *buffer, uint16_t total_length);
int uhci_set_configuration(uint16_t io_base, uint8_t device_address, uint8_t configuration_value);

// Enumeration
void uhci_enumerate_device(uint16_t io_base, int port);
void uhci_enumerate_devices(usb_controller_t *controller);

void uhci_reset_port(uint16_t io_base, int port);

int uhci_kbd_open_interrupt_in(uint16_t io_base,
                                uint8_t dev_addr,
                                uint8_t endpoint_address,
                                uint8_t interval_frames,
                                uint16_t wMaxPacket,
                                int keyboard_dev_index);

typedef struct {
    uint32_t link_pointer;
    uint32_t control_status;
    uint32_t token;
    uint32_t buffer_pointer;
} __attribute__((packed, aligned(16))) uhci_td_t;

typedef struct {
    uint32_t horizontal_link_pointer;
    uint32_t vertical_link_pointer;
} __attribute__((packed, aligned(16))) uhci_qh_t;

// Shared frame list (defined in controller.c)
extern uint32_t frame_list[1024];

#endif // UHCI_UHCI_H
