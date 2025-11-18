#include <stdint.h>
#include <stddef.h>
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
#include "drivers/usb/usb.h"
#include "kernel/include/kernel/bootinfo.h"
#include "drivers/screen/framebuffer_console.h"

extern kernel_bootinfo_t kernel_bootinfo;

void kernel_main() {
    cpu_enable_fpu_sse();
    isr_install();
    if ((kernel_bootinfo.flags & KERNEL_BOOTINFO_FLAG_UEFI) == 0) {
        irq_install();
    }
    bool fb_ready = framebuffer_console_init(&kernel_bootinfo);
    if (!fb_ready) {
        screen_set_available(true);
    }
    if (fb_ready) {
        screen_draw_rect(50, 50, 200, 200, 0x00FF0000); /* BGR: red */
    }
    if (screen_is_available()) {
        clear_screen();
    }
    pci_scan();
    pci_scan_for_usb_controllers();
    usb_enumerate_devices();
    kbd_subsystem_init();

    
    while(true){
        shell_main_loop();
    }
}
