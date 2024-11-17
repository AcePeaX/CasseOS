#include "../drivers/screen.h"
#include "../libc/string.h"
#include "cpu/isr.h"
#include "cpu/idt.h"
#include "../libc/system.h"
#include "../cpu/timer.h"
#include "../drivers/keyboard.h"
#include "shell/shell.h"

void kernel_main() {
    isr_install();
    irq_install();
    clear_screen();

    
    while(true){
        shell_main_loop();
    }
}

//extern uint8_t keyboard_auto_display;
