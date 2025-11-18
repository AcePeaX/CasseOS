#include <stddef.h>
#include <stdint.h>
#include "framebuffer_console.h"
#include "../screen.h"

static framebuffer_console_t fb_console;
static bool fb_console_ready = false;

bool framebuffer_console_init(const kernel_bootinfo_t *bootinfo) {
    if (!bootinfo) {
        fb_console_ready = false;
        return false;
    }
    if ((bootinfo->flags & KERNEL_BOOTINFO_FLAG_FRAMEBUFFER) == 0) {
        fb_console_ready = false;
        return false;
    }
    if (bootinfo->fb_base == 0 || bootinfo->fb_width == 0 ||
        bootinfo->fb_height == 0 || bootinfo->fb_stride == 0) {
        fb_console_ready = false;
        return false;
    }

    fb_console.base = (volatile uint32_t *)(uintptr_t)bootinfo->fb_base;
    fb_console.size = bootinfo->fb_size;
    fb_console.width = bootinfo->fb_width;
    fb_console.height = bootinfo->fb_height;
    fb_console.stride = bootinfo->fb_stride;
    fb_console.bpp = bootinfo->fb_bpp;
    fb_console_ready = true;

    /* Disable VGA text rendering once the framebuffer console is ready. */
    screen_set_available(false);
    return true;
}

bool framebuffer_console_is_ready(void) {
    return fb_console_ready;
}

const framebuffer_console_t *framebuffer_console_info(void) {
    return fb_console_ready ? &fb_console : NULL;
}
