#ifndef CASSEOS_DRIVERS_FRAMEBUFFER_CONSOLE_H
#define CASSEOS_DRIVERS_FRAMEBUFFER_CONSOLE_H

#include <stdbool.h>
#include <stdint.h>
#include "kernel/include/kernel/bootinfo.h"

typedef struct {
    volatile uint32_t *base;
    uint64_t size;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    uint32_t bpp;
} framebuffer_console_t;

bool framebuffer_console_init(const kernel_bootinfo_t *bootinfo);
bool framebuffer_console_is_ready(void);
const framebuffer_console_t *framebuffer_console_info(void);

#endif /* CASSEOS_DRIVERS_FRAMEBUFFER_CONSOLE_H */
