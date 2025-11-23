#ifndef SYSTEM_H
#define SYSTEM_H

#include <stdint.h>

#define QEMU_ACPI_SHUTDOWN_PORT 0x604
#define QEMU_DEBUG_PORT 0xE9

void debug_putc(char c);
void debug_puts(const char *s);
void debug_puthex(uint32_t val);
void shutdown();

#endif
