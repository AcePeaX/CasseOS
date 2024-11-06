#include "../drivers/screen.h"
#include "../libc/string.h"
#include "../libc/system.h"
#include "../cpu/isr.h"
#include "../cpu/idt.h"
#include "../cpu/timer.h"
#include "../drivers/keyboard.h"

void kernel_main() {
    isr_install();
    irq_install();
    clear_screen();
    kprint("Welcome to CasseOS!\n>");

}

extern uint8_t keyboard_auto_display;

void user_input(char *input) {
    if (strcmp(input, "end") == 0) {
        kprint("Stopping the CPU. Bye!\n");
        shutdown();
    } else if (strcmp(input, "page") == 0) {
        /* Lesson 22: Code to test kmalloc, the rest is unchanged */
        /*uint32_t phys_addr;
        uint32_t page = kmalloc(1000, 1, &phys_addr);
        char page_str[16] = "";
        hex_to_ascii(page, page_str);
        char phys_str[16] = "";
        hex_to_ascii(phys_addr, phys_str);
        kprint("Page: ");
        kprint(page_str);
        kprint(", physical address: ");
        kprint(phys_str);
        kprint("\n");*/
    }
    else if (strcmp(input, "ghost") == 0) {
        keyboard_auto_display = 1 - keyboard_auto_display;
    }
    else if (strcmp(input, "keyboard azerty") == 0) {
        set_keyboard_type(1);
    }
    else if (strcmp(input, "keyboard qwerty") == 0) {
        set_keyboard_type(0);
    }
    kprint("You said: ");
    kprint(input);
    kprint("\n> ");
}