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
    fb_console.font = framebuffer_font8x16;
    fb_console.glyph_width = FRAMEBUFFER_FONT_WIDTH;
    fb_console.glyph_height = FRAMEBUFFER_FONT_HEIGHT;
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

bool framebuffer_console_draw_glyph(char c, uint32_t x, uint32_t y,
                                    uint32_t fg_color, uint32_t bg_color) {
    if (!fb_console_ready || fb_console.bpp != 32) {
        return false;
    }

    uint32_t glyph_w = fb_console.glyph_width;
    uint32_t glyph_h = fb_console.glyph_height;
    if (glyph_w == 0 || glyph_h == 0) {
        return false;
    }

    if (x >= fb_console.width || y >= fb_console.height) {
        return false;
    }
    if (x + glyph_w > fb_console.width || y + glyph_h > fb_console.height) {
        return false;
    }

    const uint8_t *glyph = framebuffer_font_get_glyph((uint32_t)(uint8_t)c);
    if (!glyph) {
        return false;
    }

    for (uint32_t row = 0; row < glyph_h; ++row) {
        uint32_t screen_y = y + row;
        volatile uint32_t *pixel = fb_console.base + screen_y * fb_console.stride + x;
        uint8_t bits = glyph[row];
        for (uint32_t col = 0; col < glyph_w; ++col) {
            uint32_t color = (bits & (0x80u >> col)) ? fg_color : bg_color;
            pixel[col] = color;
        }
    }

    return true;
}
