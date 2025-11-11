#ifndef USB_CORE_H
#define USB_CORE_H

#include <stdint.h>
#include "../pci.h"
#include "../screen.h"
#include "usb_descriptors.h"

typedef enum {
    USB_LOG_LEVEL_NONE  = 0,
    USB_LOG_LEVEL_TRACE = 1,
    USB_LOG_LEVEL_DEBUG = 2,
    USB_LOG_LEVEL_INFO  = 3,
    USB_LOG_LEVEL_WARN  = 4,
    USB_LOG_LEVEL_ERROR = 5,
} usb_log_level_t;

#ifndef USB_LOG_LEVEL
#define USB_LOG_LEVEL USB_LOG_LEVEL_WARN
#endif

#define USB_LOG_ENABLED(level) ((level) >= USB_LOG_LEVEL)

#define USB_LOG_PRINT(level, level_tag, fmt, ...) \
    do { if (USB_LOG_ENABLED(level)) printf("[USB]" level_tag " " fmt, ##__VA_ARGS__); } while (0)

#define USB_LOG_TRACE(fmt, ...) USB_LOG_PRINT(USB_LOG_LEVEL_TRACE, "[TRC]", fmt, ##__VA_ARGS__)
#define USB_LOG_DEBUG(fmt, ...) USB_LOG_PRINT(USB_LOG_LEVEL_DEBUG, "[DBG]", fmt, ##__VA_ARGS__)
#define USB_LOG_INFO(fmt, ...)  USB_LOG_PRINT(USB_LOG_LEVEL_INFO,  "[INF]", fmt, ##__VA_ARGS__)
#define USB_LOG_WARN(fmt, ...)  USB_LOG_PRINT(USB_LOG_LEVEL_WARN,  "[WRN]", fmt, ##__VA_ARGS__)
#define USB_LOG_ERROR(fmt, ...) USB_LOG_PRINT(USB_LOG_LEVEL_ERROR, "[ERR]", fmt, ##__VA_ARGS__)

#define USB_SUBSYS_LOG(level, subsystem_level, subsystem_tag, level_tag, fmt, ...) \
    do { \
        if (((level) >= (subsystem_level)) && USB_LOG_ENABLED(level)) { \
            printf("[USB]" subsystem_tag level_tag " " fmt, ##__VA_ARGS__); \
        } \
    } while (0)

typedef struct {
    pci_device_t *pci_device;
    uint32_t base_address; // Base address for memory-mapped or port-mapped I/O
} usb_controller_t;

extern usb_controller_t usb_controllers[16]; // Support up to 16 controllers
extern uint8_t usb_controller_count;

void pci_scan_for_usb_controllers();
void usb_enumerate_devices();
//void usb_init();

#endif
