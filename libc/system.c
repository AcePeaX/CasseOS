#include "system.h"
#include "../cpu/ports.h"

void shutdown(){
    port_word_out(QEMU_ACPI_SHUTDOWN_PORT, (char)0x2000);
    asm volatile ("cli");
    asm volatile ("hlt");
}