#ifndef USB_DESCRIPTORS_H
#define USB_DESCRIPTORS_H

#include <stdint.h>

/* USB Descriptor Types */
#define USB_DESC_TYPE_DEVICE         0x01
#define USB_DESC_TYPE_CONFIGURATION  0x02
#define USB_DESC_TYPE_STRING         0x03
#define USB_DESC_TYPE_INTERFACE      0x04
#define USB_DESC_TYPE_ENDPOINT       0x05
#define USB_DESC_TYPE_HID            0x21
#define USB_DESC_TYPE_REPORT         0x22

/* USB Device Classes */
#define USB_CLASS_HID                0x03
#define USB_SUBCLASS_BOOT            0x01
#define USB_PROTOCOL_KEYBOARD        0x01

/* Device Descriptor */
typedef struct {
    uint8_t length;           // Descriptor size (18 bytes)
    uint8_t descriptor_type;  // Type of descriptor (1 = device descriptor)
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
    uint8_t num_configurations; // Number of configurations
} __attribute__((packed)) usb_device_descriptor_t;

/* Configuration Descriptor */
typedef struct {
    uint8_t length;              // Descriptor size (9 bytes)
    uint8_t descriptor_type;     // Type of descriptor (2 = configuration)
    uint16_t total_length;       // Total length of data for this configuration
    uint8_t num_interfaces;      // Number of interfaces
    uint8_t configuration_value; // Value to use as an argument for SetConfiguration
    uint8_t configuration_index; // Index of configuration string descriptor
    uint8_t attributes;          // Configuration characteristics
    uint8_t max_power;           // Maximum power consumption (in 2mA units)
} __attribute__((packed)) usb_configuration_descriptor_t;

/* Interface Descriptor */
typedef struct {
    uint8_t length;              // Descriptor size (9 bytes)
    uint8_t descriptor_type;     // Type of descriptor (4 = interface)
    uint8_t interface_number;    // Number of this interface
    uint8_t alternate_setting;   // Value to select alternate setting
    uint8_t num_endpoints;       // Number of endpoints used by this interface
    uint8_t interface_class;     // Interface class
    uint8_t interface_subclass;  // Interface subclass
    uint8_t interface_protocol;  // Interface protocol
    uint8_t interface_index;     // Index of interface string descriptor
} __attribute__((packed)) usb_interface_descriptor_t;

/* Endpoint Descriptor */
typedef struct {
    uint8_t length;              // Descriptor size (7 bytes)
    uint8_t descriptor_type;     // Type of descriptor (5 = endpoint)
    uint8_t endpoint_address;    // Endpoint address (direction + endpoint number)
    uint8_t attributes;          // Endpoint attributes (e.g., transfer type)
    uint16_t max_packet_size;    // Maximum packet size
    uint8_t interval;            // Polling interval (for interrupt and isochronous transfers)
} __attribute__((packed)) usb_endpoint_descriptor_t;

/* HID Descriptor (for HID devices) */
typedef struct {
    uint8_t length;              // Descriptor size
    uint8_t descriptor_type;     // Type of descriptor (0x21 = HID)
    uint16_t hid_version;        // HID specification release
    uint8_t country_code;        // Country code
    uint8_t num_descriptors;     // Number of additional descriptors
    uint8_t report_type;         // Descriptor type (0x22 = report descriptor)
    uint16_t report_length;      // Length of report descriptor
} __attribute__((packed)) usb_hid_descriptor_t;

#define MAX_USB_DEVICES 128

typedef struct {
    uint8_t address; // USB address assigned to the device
    usb_device_descriptor_t descriptor;
    usb_configuration_descriptor_t config_descriptor;
    usb_interface_descriptor_t interface_descriptor;
    usb_endpoint_descriptor_t endpoint_descriptors[16]; // Maximum endpoints
} usb_device_t;

extern usb_device_t usb_devices[MAX_USB_DEVICES];
extern uint8_t usb_device_count;

typedef struct {
    uint8_t bmRequestType;
    uint8_t bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
} __attribute__((packed)) usb_setup_packet_t;

#endif
