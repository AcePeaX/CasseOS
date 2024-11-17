#ifndef PORTS_H
#define PORTS_H

#include <stdint.h>

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA    0xCFC

#define PCI_CLASS_SERIAL    0x0C
#define PCI_SUBCLASS_USB    0x03


#include <stdint.h>

/**
 * Read a byte from the specified port
 */
static inline uint8_t port_byte_in(uint16_t port) {
    uint8_t result;
    asm volatile("in %%dx, %%al" : "=a"(result) : "d"(port));
    return result;
}

/**
 * Write a byte to the specified port
 */
static inline void port_byte_out(uint16_t port, uint8_t data) {
    asm volatile("out %%al, %%dx" : : "a"(data), "d"(port));
}

/**
 * Read a word (2 bytes) from the specified port
 */
static inline uint16_t port_word_in(uint16_t port) {
    uint16_t result;
    asm volatile("in %%dx, %%ax" : "=a"(result) : "d"(port));
    return result;
}

/**
 * Write a word (2 bytes) to the specified port
 */
static inline void port_word_out(uint16_t port, uint16_t data) {
    asm volatile("out %%ax, %%dx" : : "a"(data), "d"(port));
}

/**
 * Read a double word (4 bytes) from the specified port
 */
static inline uint32_t port_dword_in(uint16_t port) {
    uint32_t result;
    asm volatile("in %%dx, %%eax" : "=a"(result) : "d"(port));
    return result;
}

/**
 * Write a double word (4 bytes) to the specified port
 */
static inline void port_dword_out(uint16_t port, uint32_t data) {
    asm volatile("out %%eax, %%dx" : : "a"(data), "d"(port));
}

/* Alternative names for the functions using macros */
#define io_byte_in port_byte_in
#define io_byte_out port_byte_out
#define io_word_in port_word_in
#define io_word_out port_word_out
#define io_dword_in port_dword_in
#define io_dword_out port_dword_out

#endif
