#include <stdarg.h>
#include <stdint.h>
#include "../screen.h"
#include "cpu/ports.h"
#include "libc/string.h"
#include "libc/mem.h"
#include "framebuffer_console.h"

/* Declaration of private functions */
int get_cursor_offset();
void set_cursor_offset(int offset);
int print_char(char c, int col, int row, char attr);
int get_offset(int col, int row);
int get_vga_offset_row(int offset);
int get_vga_offset_col(int offset);

static bool screen_available = true;
bool screen_auto_cursor = true;

typedef struct {
    bool active;
    uint32_t cell_width;
    uint32_t cell_height;
    uint32_t cols;
    uint32_t rows;
    uint32_t cursor_col;
    uint32_t cursor_row;
    uint32_t fg_color;
    uint32_t bg_color;
} fb_console_state_t;

static fb_console_state_t fb_console_state = {
    .active = false,
    .cell_width = 0,
    .cell_height = 0,
    .cols = 0,
    .rows = 0,
    .cursor_col = 0,
    .cursor_row = 0,
    .fg_color = 0x00FFFFFF,
    .bg_color = 0x00000000,
};

static bool fb_console_use(void);
static void fb_console_clear(void);
static void fb_console_scroll(void);
static int fb_console_print_char(char c, int col, int row);
static void fb_console_fill_rect(uint32_t x, uint32_t y, uint32_t width,
                                 uint32_t height, uint32_t color);

void screen_set_available(bool available) {
    screen_available = available;
}

bool screen_is_available(void) {
    return screen_available || framebuffer_console_is_ready();
}

bool screen_draw_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color) {
    const framebuffer_console_t *fb = framebuffer_console_info();
    if (!fb) {
        return false;
    }

    uint32_t max_x = (x + width > fb->width) ? fb->width : x + width;
    uint32_t max_y = (y + height > fb->height) ? fb->height : y + height;
    volatile uint32_t *fb_base = fb->base;

    for (uint32_t row = y; row < max_y; ++row) {
        volatile uint32_t *line = fb_base + row * fb->stride + x;
        for (uint32_t col = x; col < max_x; ++col) {
            line[col - x] = color;
        }
    }
    return true;
}

void set_auto_cursor(bool auto_cursor){
    screen_auto_cursor=auto_cursor;
}
bool get_auto_cursor(){
    return screen_auto_cursor;
}

/**********************************************************
 * Public Kernel API functions                            *
 **********************************************************/

/**
 * Print a message on the specified location
 * If col, row, are negative, we will use the current offset
 */
void kprint_at(char *message, int col, int row) {
    if (!screen_available && !framebuffer_console_is_ready()) {
        return;
    }
    /* Set cursor if col/row are negative */
    int offset;
    if (col >= 0 && row >= 0)
        offset = get_offset(col, row);
    else {
        offset = get_cursor_offset();
        row = get_vga_offset_row(offset);
        col = get_vga_offset_col(offset);
    }

    /* Loop through message and print it */
    int i = 0;
    while (message[i] != 0) {
        offset = print_char(message[i++], col, row, WHITE_ON_BLACK);
        /* Compute row/col for next iteration */
        row = get_vga_offset_row(offset);
        col = get_vga_offset_col(offset);
    }
}

void kprint(char *message) {
    if (!screen_available && !framebuffer_console_is_ready()) {
        return;
    }
    kprint_at(message, -1, -1);
}

void kprint_backspace() {
    if (!screen_available && !framebuffer_console_is_ready()) {
        return;
    }
    int offset = get_cursor_offset()-2;
    int row = get_vga_offset_row(offset);
    int col = get_vga_offset_col(offset);
    print_char(0x00, col, row, WHITE_ON_BLACK);
    set_cursor_offset(offset);
}


/**********************************************************
 * Private kernel functions                               *
 **********************************************************/


/**
 * Innermost print function for our kernel, directly accesses the video memory 
 *
 * If 'col' and 'row' are negative, we will print at current cursor location
 * If 'attr' is zero it will use 'white on black' as default
 * Returns the offset of the next character
 * Sets the video cursor to the returned offset
 */
int print_char(char c, int col, int row, char attr) {
    if (framebuffer_console_is_ready() && fb_console_use()) {
        int offset = fb_console_print_char(c, col, row);
        if (screen_auto_cursor) {
            set_cursor_offset(offset);
        }
        return offset;
    }
    if (!screen_available) {
        return get_offset(col, row);
    }
    unsigned char *vidmem = (unsigned char*) VIDEO_ADDRESS;
    if (!attr) attr = WHITE_ON_BLACK;

    /* Error control: print a red 'E' if the coords aren't right */
    if (col >= MAX_COLS || row >= MAX_ROWS) {
        vidmem[2*(MAX_COLS)*(MAX_ROWS)-2] = 'E';
        vidmem[2*(MAX_COLS)*(MAX_ROWS)-1] = RED_ON_WHITE;
        return get_offset(col, row);
    }

    int offset;
    if (col >= 0 && row >= 0) offset = get_offset(col, row);
    else offset = get_cursor_offset();

    if (c == '\n') {
        row = get_vga_offset_row(offset);
        offset = get_offset(0, row+1);
    } else {
        vidmem[offset] = c;
        vidmem[offset+1] = attr;
        offset += 2;
    }

    /* Check if the offset is over screen size and scroll */
    if (offset >= MAX_ROWS * MAX_COLS * 2) {
        int i;
        for (i = 1; i < MAX_ROWS; i++) 
            memory_copy((uint8_t*)(get_offset(0, i-1) + VIDEO_ADDRESS),
                        (uint8_t*)(get_offset(0, i) + VIDEO_ADDRESS),
                        MAX_COLS * 2);

        /* Blank last line */
        char *last_line = (char*) (get_offset(0, MAX_ROWS-1) + (uint8_t*)VIDEO_ADDRESS);
        for (i = 0; i < MAX_COLS * 2; i++) last_line[i] = 0;

        offset -= 2 * MAX_COLS;
    }

    if(screen_auto_cursor){
        set_cursor_offset(offset);
    }
    return offset;
}

int get_cursor_offset() {
    if (framebuffer_console_is_ready() && fb_console_use()) {
        int cols = (int)fb_console_state.cols;
        if (cols <= 0) {
            return 0;
        }
        int index = (int)(fb_console_state.cursor_row * cols + fb_console_state.cursor_col);
        return index * 2;
    }
    if (!screen_available) {
        return 0;
    }
    /* Use the VGA ports to get the current cursor position
     * 1. Ask for high byte of the cursor offset (data 14)
     * 2. Ask for low byte (data 15)
     */
    port_byte_out(REG_SCREEN_CTRL, 14);
    int offset = port_byte_in(REG_SCREEN_DATA) << 8; /* High byte: << 8 */
    port_byte_out(REG_SCREEN_CTRL, 15);
    offset += port_byte_in(REG_SCREEN_DATA);
    return offset * 2; /* Position * size of character cell */
}

void set_cursor_offset(int offset) {
    if (framebuffer_console_is_ready() && fb_console_use()) {
        if (offset < 0) offset = 0;
        int cols = (int)fb_console_state.cols;
        if (cols <= 0) {
            return;
        }
        int index = offset / 2;
        int row = index / cols;
        int col = index % cols;
        if (row < 0) row = 0;
        if (col < 0) col = 0;
        if (row >= (int)fb_console_state.rows) {
            row = (int)fb_console_state.rows - 1;
        }
        if (col >= (int)fb_console_state.cols) {
            col = (int)fb_console_state.cols - 1;
        }
        fb_console_state.cursor_row = (uint32_t)row;
        fb_console_state.cursor_col = (uint32_t)col;
        return;
    }
    if (!screen_available) {
        return;
    }
    /* Similar to get_cursor_offset, but instead of reading we write data */
    offset /= 2;
    port_byte_out(REG_SCREEN_CTRL, 14);
    port_byte_out(REG_SCREEN_DATA, (unsigned char)(offset >> 8));
    port_byte_out(REG_SCREEN_CTRL, 15);
    port_byte_out(REG_SCREEN_DATA, (unsigned char)(offset & 0xff));
}

void clear_screen() {
    if (framebuffer_console_is_ready() && fb_console_use()) {
        fb_console_clear();
        return;
    }
    if (!screen_available) {
        return;
    }
    int screen_size = MAX_COLS * MAX_ROWS;
    int i;
    char *screen = VIDEO_ADDRESS;

    for (i = 0; i < screen_size; i++) {
        screen[i*2] = ' ';
        screen[i*2+1] = WHITE_ON_BLACK;
    }
    set_cursor_offset(get_offset(0, 0));
}


int get_offset(int col, int row) {
    if (framebuffer_console_is_ready() && fb_console_use()) {
        if (col < 0) col = 0;
        if (row < 0) row = 0;
        int cols = (int)fb_console_state.cols;
        int rows = (int)fb_console_state.rows;
        if (cols <= 0 || rows <= 0) {
            return 0;
        }
        if (col >= cols) col = cols - 1;
        if (row >= rows) row = rows - 1;
        return 2 * (row * cols + col);
    }
    return 2 * (row * MAX_COLS + col);
}

int get_vga_offset_row(int offset) {
    if (framebuffer_console_is_ready() && fb_console_use()) {
        int cols = (int)fb_console_state.cols;
        if (cols <= 0) {
            return 0;
        }
        int index = offset / 2;
        return index / cols;
    }
    return offset / (2 * MAX_COLS);
}

int get_vga_offset_col(int offset) {
    if (framebuffer_console_is_ready() && fb_console_use()) {
        int cols = (int)fb_console_state.cols;
        if (cols <= 0) {
            return 0;
        }
        int index = offset / 2;
        return index % cols;
    }
    return (offset - (get_vga_offset_row(offset)*2*MAX_COLS))/2;
}


void printf(const char *format, ...) {
    if (!screen_available && !framebuffer_console_is_ready()) {
        return;
    }
    va_list args; // List of variable arguments
    va_start(args, format); // Initialize the list with the format string

    char buffer[256]; // Temporary buffer for formatted output
    int buffer_index = 0;

    for (int i = 0; format[i] != '\0'; i++) {
        if (format[i] == '%' && format[i + 1] != '\0') {
            i++; // Move to the specifier
            switch (format[i]) {
                case 'd': { // Integer
                    int value = va_arg(args, int);
                    char int_buffer[16];
                    int_to_ascii(value, int_buffer);
                    for (int j = 0; int_buffer[j] != '\0'; j++) {
                        buffer[buffer_index++] = int_buffer[j];
                    }
                    break;
                }
                case 'u': {
                    unsigned int value = va_arg(args, unsigned int);
                    char uint_buffer[16];
                    uint_to_ascii(value, uint_buffer);  // ← you'll need this small helper
                    for (int j = 0; uint_buffer[j] != '\0'; j++)
                        buffer[buffer_index++] = uint_buffer[j];
                    break;
                }
                case 'x': { // Hexadecimal
                    uint64_t value = va_arg(args, uint64_t);
                    char hex_buffer[17];
                    hex_to_string_trimmed(value, hex_buffer);
                    for (int j = 0; hex_buffer[j] != '\0'; j++) {
                        buffer[buffer_index++] = hex_buffer[j];
                    }
                    break;
                }
                case 'b': { // Binary with insignificant zeros removed
                    uint32_t value = va_arg(args, uint32_t);
                    char binary_buffer[33]; // Max 32 bits + null terminator
                    int started = 0; // Flag to track if we've encountered the first '1'
                    int buffer_pos = 0;

                    for (int j = 31; j >= 0; j--) {
                        char bit = (value & (1 << j)) ? '1' : '0';
                        if (bit == '1') {
                            started = 1;
                        }
                        if (started) {
                            binary_buffer[buffer_pos++] = bit;
                        }
                    }

                    if (!started) {
                        // If no '1' was encountered, the value is 0
                        binary_buffer[buffer_pos++] = '0';
                    }

                    binary_buffer[buffer_pos] = '\0'; // Null-terminate the string

                    for (int j = 0; binary_buffer[j] != '\0'; j++) {
                        buffer[buffer_index++] = binary_buffer[j];
                    }
                    break;
                }
                case 's': { // String
                    char *str = va_arg(args, char *);
                    for (int j = 0; str[j] != '\0'; j++) {
                        buffer[buffer_index++] = str[j];
                    }
                    break;
                }
                case 'c': { // Character
                    char value = (char)va_arg(args, int);
                    buffer[buffer_index++] = value;
                    break;
                }
                case '%': { // Literal '%'
                    buffer[buffer_index++] = '%';
                    break;
                }
                default: { // Unsupported specifier
                    buffer[buffer_index++] = '%';
                    buffer[buffer_index++] = format[i];
                    break;
                }
            }
        } else {
            // Regular character
            buffer[buffer_index++] = format[i];
        }
    }

    buffer[buffer_index] = '\0'; // Null-terminate the string
    va_end(args); // Clean up

    kprint(buffer); // Print the formatted string
}

static bool fb_console_use(void) {
    if (!framebuffer_console_is_ready()) {
        fb_console_state.active = false;
        return false;
    }

    const framebuffer_console_t *fb = framebuffer_console_info();
    if (!fb) {
        fb_console_state.active = false;
        return false;
    }

    uint32_t glyph_w = fb->glyph_width;
    uint32_t glyph_h = fb->glyph_height;
    if (glyph_w == 0 || glyph_h == 0) {
        fb_console_state.active = false;
        return false;
    }

    uint32_t cell_width = glyph_w + 1;   /* 9-pixel horizontal spacing */
    uint32_t cell_height = glyph_h;      /* 16-pixel vertical spacing */
    uint32_t cols = fb->width / cell_width;
    uint32_t rows = fb->height / cell_height;
    if (cols == 0 || rows == 0) {
        fb_console_state.active = false;
        return false;
    }

    if (!fb_console_state.active) {
        fb_console_state.cursor_col = 0;
        fb_console_state.cursor_row = 0;
        fb_console_state.fg_color = 0x00FFFFFF;
        fb_console_state.bg_color = 0x00000000;
    } else {
        if (fb_console_state.cursor_col >= cols) {
            fb_console_state.cursor_col = cols - 1;
        }
        if (fb_console_state.cursor_row >= rows) {
            fb_console_state.cursor_row = rows - 1;
        }
    }

    fb_console_state.cell_width = cell_width;
    fb_console_state.cell_height = cell_height;
    fb_console_state.cols = cols;
    fb_console_state.rows = rows;
    fb_console_state.active = true;
    return true;
}

static void fb_console_fill_rect(uint32_t x, uint32_t y, uint32_t width,
                                 uint32_t height, uint32_t color) {
    const framebuffer_console_t *fb = framebuffer_console_info();
    if (!fb || width == 0 || height == 0) {
        return;
    }

    if (x >= fb->width || y >= fb->height) {
        return;
    }

    if (x + width > fb->width) {
        width = fb->width - x;
    }
    if (y + height > fb->height) {
        height = fb->height - y;
    }

    for (uint32_t row = 0; row < height; ++row) {
        volatile uint32_t *line = fb->base + (y + row) * fb->stride + x;
        for (uint32_t col = 0; col < width; ++col) {
            line[col] = color;
        }
    }
}

static void fb_console_clear(void) {
    if (!fb_console_use()) {
        return;
    }
    uint32_t width = fb_console_state.cols * fb_console_state.cell_width;
    uint32_t height = fb_console_state.rows * fb_console_state.cell_height;
    fb_console_fill_rect(0, 0, width, height, fb_console_state.bg_color);
    fb_console_state.cursor_col = 0;
    fb_console_state.cursor_row = 0;
}

static void fb_console_scroll(void) {
    if (!fb_console_use()) {
        return;
    }

    const framebuffer_console_t *fb = framebuffer_console_info();
    if (!fb) {
        return;
    }

    uint32_t cols = fb_console_state.cols;
    uint32_t rows = fb_console_state.rows;
    if (cols == 0 || rows == 0) {
        return;
    }

    uint32_t active_width = cols * fb_console_state.cell_width;
    uint32_t step = fb_console_state.cell_height;
    if (active_width == 0 || step == 0) {
        return;
    }

    uint32_t copy_height = (rows - 1) * step;
    for (uint32_t y = 0; y < copy_height; ++y) {
        volatile uint32_t *dest = fb->base + y * fb->stride;
        volatile uint32_t *src = fb->base + (y + step) * fb->stride;
        for (uint32_t x = 0; x < active_width; ++x) {
            dest[x] = src[x];
        }
    }

    for (uint32_t y = copy_height; y < copy_height + step && y < fb->height; ++y) {
        volatile uint32_t *line = fb->base + y * fb->stride;
        for (uint32_t x = 0; x < active_width; ++x) {
            line[x] = fb_console_state.bg_color;
        }
    }

    fb_console_state.cursor_row = rows - 1;
    fb_console_state.cursor_col = 0;
}

static int fb_console_print_char(char c, int col, int row) {
    if (!fb_console_use()) {
        return 0;
    }

    if (col < 0 || row < 0) {
        col = (int)fb_console_state.cursor_col;
        row = (int)fb_console_state.cursor_row;
    }

    int cols = (int)fb_console_state.cols;
    int rows = (int)fb_console_state.rows;
    if (cols <= 0 || rows <= 0) {
        return 0;
    }

    const framebuffer_console_t *fb = framebuffer_console_info();
    if (!fb) {
        return 0;
    }

    if (c == '\n') {
        col = 0;
        row += 1;
    } else {
        uint32_t px = (uint32_t)col * fb_console_state.cell_width;
        uint32_t py = (uint32_t)row * fb_console_state.cell_height;
        unsigned char glyph = (unsigned char)c;
        if (glyph == 0) {
            glyph = ' ';
        }
        framebuffer_console_draw_glyph((char)glyph, px, py,
                                       fb_console_state.fg_color,
                                       fb_console_state.bg_color);
        if (fb_console_state.cell_width > fb->glyph_width) {
            uint32_t gap_width = fb_console_state.cell_width - fb->glyph_width;
            fb_console_fill_rect(px + fb->glyph_width, py, gap_width,
                                 fb_console_state.cell_height,
                                 fb_console_state.bg_color);
        }
        col++;
    }

    if (col >= cols) {
        col = 0;
        row++;
    }

    if (row >= rows) {
        fb_console_scroll();
        row = rows - 1;
    }

    fb_console_state.cursor_col = (col < 0) ? 0 : (uint32_t)col;
    fb_console_state.cursor_row = (row < 0) ? 0 : (uint32_t)row;
    return 2 * (row * cols + col);
}
