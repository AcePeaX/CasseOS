#include "timer.h"
#include "isr.h"
#include "ports.h"
#include "drivers/screen.h"
#include "libc/function.h"

uint64_t tick = 0;
uint32_t frequency = 0;

static void timer_callback(registers_t *regs) {
    tick++;
    UNUSED(regs);
}

void init_timer(uint32_t freq) {
    /* Install the function we just wrote */
    register_interrupt_handler(IRQ0, timer_callback);

    frequency = freq;

    /* Get the PIT value: hardware clock at 1193180 Hz */
    uint32_t divisor = 1193180 / freq;
    uint8_t low  = (uint8_t)(divisor & 0xFF);
    uint8_t high = (uint8_t)( (divisor >> 8) & 0xFF);
    /* Send the command */
    port_byte_out(0x43, 0x36); /* Command port */
    port_byte_out(0x40, low);
    port_byte_out(0x40, high);

}

uint64_t timer_get_ticks(){
    return tick;
}

void sleep_ticks(uint64_t ticks){
    uint64_t start_ticks = tick;
    uint64_t end_ticks = start_ticks + ticks; // TIMER_FREQUENCY in Hz

    while (timer_get_ticks() < end_ticks) {
        asm volatile("nop"); 
    }
}

void sleep_ms(uint64_t milliseconds){
    uint64_t ticks = milliseconds * frequency / 1000;
    sleep_ticks(ticks);
}



