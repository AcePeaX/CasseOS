#include "../drivers/screen.h"
#include "../libc/string.h"
#include "cpu/isr.h"
#include "cpu/idt.h"
#include "../libc/system.h"
#include "../cpu/timer.h"
#include "../drivers/keyboard/keyboard.h"
#include "shell/shell.h"
#include "drivers/pci.h"

void kernel_main() {
    isr_install();
    irq_install();
    clear_screen();
    pci_scan();
    pci_scan_for_usb_controllers();
    usb_enumerate_devices();
    //keyboard_subsystem_init();

    
    while(true){
        shell_main_loop();
    }
}

//extern uint8_t keyboard_auto_display;
