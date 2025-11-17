#include "libc/string.h"
#include "libc/system.h"
#include "cpu/type.h"
#include "cpu/isr.h"
#include "cpu/idt.h"
#include "cpu/timer.h"
#include "drivers/screen.h"
#include "drivers/keyboard/keyboard.h"
#include "shell/shell.h"
#include "drivers/pci.h"
#include "kernel/include/kernel/bootinfo.h"

extern kernel_bootinfo_t kernel_bootinfo;

void kernel_main() {
    cpu_enable_fpu_sse();
    isr_install();
    if ((kernel_bootinfo.flags & KERNEL_BOOTINFO_FLAG_UEFI) == 0) {
        irq_install();
    }
    clear_screen();
    pci_scan();
    pci_scan_for_usb_controllers();
    usb_enumerate_devices();
    kbd_subsystem_init();

    
    while(true){
        shell_main_loop();
    }
}
