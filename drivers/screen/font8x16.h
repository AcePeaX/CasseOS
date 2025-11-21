#ifndef CASSEOS_DRIVERS_SCREEN_FONT8X16_H
#define CASSEOS_DRIVERS_SCREEN_FONT8X16_H

#include <stdint.h>

#define FRAMEBUFFER_FONT_WIDTH 8
#define FRAMEBUFFER_FONT_HEIGHT 16

extern const uint8_t framebuffer_font8x16[256][16];

const uint8_t *framebuffer_font_get_glyph(uint32_t codepoint);

#endif /* CASSEOS_DRIVERS_SCREEN_FONT8X16_H */
