#include "system.h"
#include "../cpu/ports.h"

void debug_putc(char c) {
    port_byte_out(QEMU_DEBUG_PORT, (uint8_t)c);
}

void debug_puts(const char *s) {
    if (!s) return;
    while (*s) {
        debug_putc(*s++);
    }
}

static void debug_puthex_nibble(uint8_t v) {
    v &= 0xF;
    debug_putc((v < 10) ? ('0' + v) : ('A' + (v - 10)));
}

void debug_puthex(uint32_t val) {
    debug_puts("0x");
    for (int i = 7; i >= 0; --i) {
        debug_puthex_nibble((uint8_t)(val >> (i * 4)));
    }
}

void shutdown(){
    port_word_out(QEMU_ACPI_SHUTDOWN_PORT, (char)0x2000);
    asm volatile ("cli");
    asm volatile ("hlt");
}
