#ifndef IDT_H
#define IDT_H

#include <stdint.h>

/* Segment selectors */
#define KERNEL_CS 0x08

/* How every interrupt gate (handler) is defined */
typedef struct {
    uint16_t low_offset;    /* Lower 16 bits of handler function address */
    uint16_t sel;           /* Kernel segment selector */
    uint8_t ist;            /* Bits 0-2: Interrupt Stack Table offset, bits 3-7: reserved (set to 0) */
    uint8_t type_attr;      /* Type and attributes */
    uint16_t mid_offset;    /* Middle 16 bits of handler function address */
    uint32_t high_offset;   /* Higher 32 bits of handler function address */
    uint32_t zero;          /* Reserved, set to 0 */
} __attribute__((packed)) idt_gate_t;


/* A pointer to the array of interrupt handlers.
 * Assembly instruction 'lidt' will read it */
typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) idt_register_t;

#define IDT_ENTRIES 256

/* Declare the IDT and IDT register as external */
extern idt_gate_t idt[IDT_ENTRIES];
extern idt_register_t idt_reg;


/* Functions implemented in idt.c */
void set_idt_gate(int n, uint64_t handler);
void set_idt();

#endif
