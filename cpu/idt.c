#include "idt.h"
#include "type.h"

idt_gate_t idt[IDT_ENTRIES];
idt_register_t idt_reg;

void set_idt_gate(int num, uint64_t handler) {
    idt[num].low_offset = handler & 0xFFFF;
    idt[num].mid_offset = (handler >> 16) & 0xFFFF;
    idt[num].high_offset = (handler >> 32) & 0xFFFFFFFF;
    idt[num].sel = 0x08;       // Kernel code segment selector
    idt[num].ist = 0;          // No IST
    idt[num].type_attr = 0x8E; // Present, privilege level 0, 64-bit interrupt gate
    idt[num].zero = 0;         // Reserved, must be 0
}


void set_idt() {
    idt_reg.base = (uint64_t) &idt;
    idt_reg.limit = IDT_ENTRIES * sizeof(idt_gate_t) - 1;
    /* Don't make the mistake of loading &idt -- always load &idt_reg */
    asm volatile("lidt (%0)" : : "r" (&idt_reg));
}
