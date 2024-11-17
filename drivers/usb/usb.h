#ifndef USB_CORE_H
#define USB_CORE_H

#include <stdint.h>

#define USB_DESCRIPTOR_TYPE_DEVICE        1
#define USB_DESCRIPTOR_TYPE_CONFIGURATION 2
#define USB_DESCRIPTOR_TYPE_INTERFACE     4
#define USB_DESCRIPTOR_TYPE_ENDPOINT      5

#define USB_CLASS_HID                     0x03
#define USB_SUBCLASS_BOOT_INTERFACE       0x01
#define USB_PROTOCOL_KEYBOARD             0x01

/* USB Device Descriptor */
typedef struct {
    uint8_t length;           // Descriptor size in bytes (18 for device descriptor)
    uint8_t descriptor_type;  // Descriptor type (1 = device descriptor)
    uint16_t usb_version;     // USB specification version
    uint8_t device_class;     // Device class
    uint8_t device_subclass;  // Device subclass
    uint8_t device_protocol;  // Protocol code
    uint8_t max_packet_size;  // Max packet size for endpoint 0
    uint16_t vendor_id;       // Vendor ID
    uint16_t product_id;      // Product ID
    uint16_t device_version;  // Device version
    uint8_t manufacturer;     // Index of manufacturer string descriptor
    uint8_t product;          // Index of product string descriptor
    uint8_t serial_number;    // Index of serial number string descriptor
    uint8_t num_configurations; // Number of possible configurations
} __attribute__((packed)) usb_device_descriptor_t;

/* USB Endpoint Descriptor */
typedef struct {
    uint8_t length;
    uint8_t descriptor_type;
    uint8_t endpoint_address;
    uint8_t attributes;
    uint16_t max_packet_size;
    uint8_t interval;
} __attribute__((packed)) usb_endpoint_descriptor_t;

/* USB Configuration Descriptor */
typedef struct {
    uint8_t length;
    uint8_t descriptor_type;
    uint16_t total_length;
    uint8_t num_interfaces;
    uint8_t configuration_value;
    uint8_t configuration_index;
    uint8_t attributes;
    uint8_t max_power;
} __attribute__((packed)) usb_configuration_descriptor_t;

//void usb_init();

#endif