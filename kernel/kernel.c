#include "../drivers/screen.h"
#include "../libc/string.h"
/*#include "../libc/system.h"
#include "../cpu/isr.h"
#include "../cpu/idt.h"
#include "../cpu/timer.h"
#include "../drivers/keyboard.h"
#include "shell/shell.h"*/

void kernel_main() {
    clear_screen();
    kprint("Welcome to CasseOS:\n> ");
    while(1){
        
    }

}

//extern uint8_t keyboard_auto_display;
