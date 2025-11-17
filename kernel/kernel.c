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
#include "kernel/include/kernel/bootinfo.h"

extern kernel_bootinfo_t kernel_bootinfo;

static void framebuffer_fill_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color) {
    if ((kernel_bootinfo.flags & KERNEL_BOOTINFO_FLAG_FRAMEBUFFER) == 0) {
        return;
    }

    uint32_t fb_width = kernel_bootinfo.fb_width;
    uint32_t fb_height = kernel_bootinfo.fb_height;
    uint32_t stride = kernel_bootinfo.fb_stride;
    if (fb_width == 0 || fb_height == 0 || stride == 0) {
        return;
    }

    uint32_t max_x = (x + width > fb_width) ? fb_width : x + width;
    uint32_t max_y = (y + height > fb_height) ? fb_height : y + height;
    volatile uint32_t *fb = (volatile uint32_t *)(uintptr_t)kernel_bootinfo.fb_base;

    for (uint32_t row = y; row < max_y; ++row) {
        volatile uint32_t *line = fb + row * stride + x;
        for (uint32_t col = x; col < max_x; ++col) {
            line[col - x] = color;
        }
    }
}

void kernel_main() {
    cpu_enable_fpu_sse();
    isr_install();
    if ((kernel_bootinfo.flags & KERNEL_BOOTINFO_FLAG_UEFI) == 0) {
        irq_install();
    }
    if (kernel_bootinfo.flags & KERNEL_BOOTINFO_FLAG_FRAMEBUFFER) {
        framebuffer_fill_rect(50, 50, 200, 200, 0x00FF0000); /* BGR: red */
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
