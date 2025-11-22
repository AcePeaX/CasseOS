#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>
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
#define PRINTF_CHUNK_SIZE 256
#define PRINTF_MAX_FLOAT_PRECISION 32

typedef struct {
    char data[PRINTF_CHUNK_SIZE];
    size_t length;
    size_t total_written;
} printf_chunk_t;

typedef enum {
    PRINTF_LEN_DEFAULT,
    PRINTF_LEN_HH,
    PRINTF_LEN_H,
    PRINTF_LEN_L,
    PRINTF_LEN_LL,
    PRINTF_LEN_Z,
    PRINTF_LEN_T,
    PRINTF_LEN_J,
    PRINTF_LEN_CAP_L
} printf_length_t;

typedef struct {
    bool left_adjust;
    bool show_sign;
    bool space_sign;
    bool alternate_form;
    bool zero_pad;
    int  width;
    int  precision;
    bool precision_specified;
    printf_length_t length;
    char specifier;
} printf_format_t;

static void printf_chunk_flush(printf_chunk_t *chunk) {
    if (chunk->length == 0) {
        return;
    }
    chunk->data[chunk->length] = '\0';
    kprint(chunk->data);
    chunk->length = 0;
}

static void printf_chunk_emit_char(printf_chunk_t *chunk, char c) {
    if (chunk->length >= PRINTF_CHUNK_SIZE - 1) {
        printf_chunk_flush(chunk);
    }
    chunk->data[chunk->length++] = c;
    chunk->total_written++;
}

static void printf_emit_repeat(printf_chunk_t *chunk, char c, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        printf_chunk_emit_char(chunk, c);
    }
}

static void printf_emit_buffer(printf_chunk_t *chunk, const char *buffer, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        printf_chunk_emit_char(chunk, buffer[i]);
    }
}

static bool printf_is_digit(char c) {
    return c >= '0' && c <= '9';
}

static int printf_parse_number(const char **fmt) {
    int value = 0;
    while (printf_is_digit(**fmt)) {
        value = value * 10 + (**fmt - '0');
        (*fmt)++;
    }
    return value;
}

static unsigned long long printf_get_unsigned_arg(va_list *args, printf_length_t length) {
    switch (length) {
        case PRINTF_LEN_HH:
            return (unsigned char)va_arg(*args, unsigned int);
        case PRINTF_LEN_H:
            return (unsigned short)va_arg(*args, unsigned int);
        case PRINTF_LEN_L:
            return va_arg(*args, unsigned long);
        case PRINTF_LEN_LL:
            return va_arg(*args, unsigned long long);
        case PRINTF_LEN_CAP_L:
            return va_arg(*args, unsigned long long);
        case PRINTF_LEN_Z:
            return va_arg(*args, size_t);
        case PRINTF_LEN_T: {
            ptrdiff_t temp = va_arg(*args, ptrdiff_t);
            return (unsigned long long)temp;
        }
        case PRINTF_LEN_J:
            return va_arg(*args, unsigned long long);
        case PRINTF_LEN_DEFAULT:
        default:
            return va_arg(*args, unsigned int);
    }
}

static long long printf_get_signed_arg(va_list *args, printf_length_t length) {
    switch (length) {
        case PRINTF_LEN_HH:
            return (signed char)va_arg(*args, int);
        case PRINTF_LEN_H:
            return (short)va_arg(*args, int);
        case PRINTF_LEN_L:
            return va_arg(*args, long);
        case PRINTF_LEN_LL:
            return va_arg(*args, long long);
        case PRINTF_LEN_Z: {
            long temp = va_arg(*args, long);
            return (long long)temp;
        }
        case PRINTF_LEN_T:
            return va_arg(*args, ptrdiff_t);
        case PRINTF_LEN_J:
            return va_arg(*args, long long);
        case PRINTF_LEN_CAP_L:
            return va_arg(*args, long long);
        case PRINTF_LEN_DEFAULT:
        default:
            return va_arg(*args, int);
    }
}

static size_t printf_convert_unsigned(unsigned long long value, unsigned base, bool uppercase, char *buffer) {
    const char *digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
    char temp[65];
    size_t len = 0;

    if (base < 2) {
        base = 10;
    }

    if (value == 0) {
        buffer[0] = '0';
        return 1;
    }

    while (value != 0) {
        temp[len++] = digits[value % base];
        value /= base;
    }

    for (size_t i = 0; i < len; ++i) {
        buffer[i] = temp[len - 1 - i];
    }
    return len;
}

static size_t printf_measure_string(const char *str, const printf_format_t *fmt) {
    size_t len = 0;
    if (!str) {
        return 0;
    }

    if (!fmt->precision_specified) {
        while (str[len] != '\0') {
            ++len;
        }
        return len;
    }

    while (str[len] != '\0' && len < (size_t)fmt->precision) {
        ++len;
    }
    return len;
}

static void printf_format_string(printf_chunk_t *chunk, const printf_format_t *fmt, const char *str) {
    if (!str) {
        str = "(null)";
    }
    size_t len = printf_measure_string(str, fmt);
    size_t padding = 0;
    if (fmt->width > (int)len) {
        padding = (size_t)(fmt->width - (int)len);
    }

    if (!fmt->left_adjust) {
        printf_emit_repeat(chunk, ' ', padding);
    }
    printf_emit_buffer(chunk, str, len);
    if (fmt->left_adjust) {
        printf_emit_repeat(chunk, ' ', padding);
    }
}

static void printf_format_char(printf_chunk_t *chunk, const printf_format_t *fmt, char value) {
    size_t padding = 0;
    if (fmt->width > 1) {
        padding = (size_t)(fmt->width - 1);
    }

    if (!fmt->left_adjust) {
        printf_emit_repeat(chunk, ' ', padding);
    }
    printf_chunk_emit_char(chunk, value);
    if (fmt->left_adjust) {
        printf_emit_repeat(chunk, ' ', padding);
    }
}

static void printf_emit_numeric_field(printf_chunk_t *chunk, const printf_format_t *fmt,
                                      const char *digits, size_t digit_len,
                                      size_t precision_zeros,
                                      char sign_char,
                                      const char *prefix,
                                      size_t prefix_len) {
    char pad_char = ' ';
    if (fmt->zero_pad && !fmt->left_adjust && !fmt->precision_specified) {
        pad_char = '0';
    }

    size_t total_len = digit_len + precision_zeros + prefix_len + (sign_char ? 1 : 0);
    size_t padding = 0;
    if (fmt->width > (int)total_len) {
        padding = (size_t)(fmt->width - (int)total_len);
    }

    if (!fmt->left_adjust) {
        if (pad_char == ' ') {
            printf_emit_repeat(chunk, pad_char, padding);
        }
        if (sign_char) {
            printf_chunk_emit_char(chunk, sign_char);
        }
        if (prefix_len > 0) {
            printf_emit_buffer(chunk, prefix, prefix_len);
        }
        if (pad_char == '0') {
            printf_emit_repeat(chunk, pad_char, padding);
        }
    } else {
        if (sign_char) {
            printf_chunk_emit_char(chunk, sign_char);
        }
        if (prefix_len > 0) {
            printf_emit_buffer(chunk, prefix, prefix_len);
        }
    }

    printf_emit_repeat(chunk, '0', precision_zeros);
    if (digit_len > 0) {
        printf_emit_buffer(chunk, digits, digit_len);
    }

    if (fmt->left_adjust && padding > 0) {
        printf_emit_repeat(chunk, ' ', padding);
    }
}

static void printf_format_integer(printf_chunk_t *chunk, printf_format_t *fmt,
                                  unsigned long long value,
                                  bool negative,
                                  unsigned base,
                                  bool uppercase,
                                  bool is_pointer,
                                  bool allow_sign) {
    char digits[65];
    size_t digit_len = 0;
    bool is_zero = (value == 0);

    if (fmt->precision_specified && fmt->precision == 0 && is_zero) {
        digit_len = 0;
    } else {
        digit_len = printf_convert_unsigned(value, base, uppercase, digits);
    }

    if ((fmt->specifier == 'o' || fmt->specifier == 'O') && fmt->alternate_form) {
        if (is_zero && digit_len == 0) {
            digits[0] = '0';
            digit_len = 1;
        }
    }

    char sign_char = 0;
    if (allow_sign) {
        if (negative) {
            sign_char = '-';
        } else if (fmt->show_sign) {
            sign_char = '+';
        } else if (fmt->space_sign) {
            sign_char = ' ';
        }
    }

    const char *prefix = NULL;
    size_t prefix_len = 0;
    if (is_pointer) {
        prefix = "0x";
        prefix_len = 2;
    } else if (fmt->alternate_form) {
        switch (fmt->specifier) {
            case 'x':
            case 'X':
                if (!is_zero) {
                    prefix = (fmt->specifier == 'X') ? "0X" : "0x";
                    prefix_len = 2;
                }
                break;
            case 'o':
            case 'O':
                if (!is_zero) {
                    prefix = "0";
                    prefix_len = 1;
                }
                break;
            case 'b':
            case 'B':
                if (!is_zero) {
                    prefix = (fmt->specifier == 'B') ? "0B" : "0b";
                    prefix_len = 2;
                }
                break;
            default:
                break;
        }
    }

    size_t precision_zeros = 0;
    if (fmt->precision_specified && fmt->precision > (int)digit_len) {
        precision_zeros = (size_t)(fmt->precision - (int)digit_len);
    }

    printf_emit_numeric_field(chunk, fmt, digits, digit_len, precision_zeros,
                              sign_char, prefix, prefix_len);
}

static void printf_output_numeric_string(printf_chunk_t *chunk, const printf_format_t *fmt,
                                         char *buffer, size_t length) {
    char pad_char = ' ';
    if (fmt->zero_pad && !fmt->left_adjust) {
        pad_char = '0';
    }

    size_t padding = 0;
    if (fmt->width > (int)length) {
        padding = (size_t)(fmt->width - (int)length);
    }

    if (!fmt->left_adjust) {
        if (pad_char == '0' && length > 0 &&
            (buffer[0] == '-' || buffer[0] == '+' || buffer[0] == ' ')) {
            printf_chunk_emit_char(chunk, buffer[0]);
            printf_emit_repeat(chunk, '0', padding);
            printf_emit_buffer(chunk, &buffer[1], length - 1);
        } else {
            printf_emit_repeat(chunk, pad_char, padding);
            printf_emit_buffer(chunk, buffer, length);
        }
    } else {
        printf_emit_buffer(chunk, buffer, length);
        printf_emit_repeat(chunk, ' ', padding);
    }
}

static void printf_format_float(printf_chunk_t *chunk, printf_format_t *fmt,
                                double value, bool uppercase) {
    if (!(value == value)) {
        const char *nan = uppercase ? "NAN" : "nan";
        char tmp[8];
        size_t idx = 0;
        if (fmt->show_sign) {
            tmp[idx++] = '+';
        } else if (fmt->space_sign) {
            tmp[idx++] = ' ';
        }
        for (size_t i = 0; nan[i] != '\0'; ++i) {
            tmp[idx++] = nan[i];
        }
        printf_output_numeric_string(chunk, fmt, tmp, idx);
        return;
    }

    if ((value * 0.0) != (value * 0.0)) {
        const char *inf = uppercase ? "INF" : "inf";
        char tmp[8];
        size_t idx = 0;
        if (value < 0.0) {
            tmp[idx++] = '-';
        } else if (fmt->show_sign) {
            tmp[idx++] = '+';
        } else if (fmt->space_sign) {
            tmp[idx++] = ' ';
        }
        for (size_t i = 0; inf[i] != '\0'; ++i) {
            tmp[idx++] = inf[i];
        }
        printf_output_numeric_string(chunk, fmt, tmp, idx);
        return;
    }

    bool negative = value < 0.0;
    if (negative) {
        value = -value;
    }

    int precision = fmt->precision_specified ? fmt->precision : 6;
    if (precision < 0) {
        precision = 6;
    }
    if (precision > PRINTF_MAX_FLOAT_PRECISION) {
        precision = PRINTF_MAX_FLOAT_PRECISION;
    }

    double rounding = 0.5;
    for (int i = 0; i < precision; ++i) {
        rounding /= 10.0;
    }
    value += rounding;

    unsigned long long integer_part = (unsigned long long)value;
    double fractional = value - (double)integer_part;

    char int_buffer[65];
    size_t int_len = printf_convert_unsigned(integer_part, 10, false, int_buffer);
    if (int_len == 0) {
        int_buffer[int_len++] = '0';
    }

    char out_buffer[PRINTF_MAX_FLOAT_PRECISION + 80];
    size_t out_len = 0;
    if (negative) {
        out_buffer[out_len++] = '-';
    } else if (fmt->show_sign) {
        out_buffer[out_len++] = '+';
    } else if (fmt->space_sign) {
        out_buffer[out_len++] = ' ';
    }

    for (size_t i = 0; i < int_len; ++i) {
        out_buffer[out_len++] = int_buffer[i];
    }

    if (precision > 0 || fmt->alternate_form) {
        out_buffer[out_len++] = '.';
        for (int i = 0; i < precision; ++i) {
            fractional *= 10.0;
            int digit = (int)fractional;
            out_buffer[out_len++] = (char)('0' + digit);
            fractional -= digit;
        }
    }

    printf_output_numeric_string(chunk, fmt, out_buffer, out_len);
}

void printf(const char *format, ...) {
    if (!screen_available && !framebuffer_console_is_ready()) {
        return;
    }

    va_list args;
    va_start(args, format);

    printf_chunk_t chunk = { .data = {0}, .length = 0, .total_written = 0 };
    const char *ptr = format;

    while (*ptr) {
        if (*ptr != '%') {
            printf_chunk_emit_char(&chunk, *ptr++);
            continue;
        }

        ++ptr;
        if (*ptr == '%') {
            printf_chunk_emit_char(&chunk, *ptr++);
            continue;
        }

        printf_format_t fmt = {
            .left_adjust = false,
            .show_sign = false,
            .space_sign = false,
            .alternate_form = false,
            .zero_pad = false,
            .width = 0,
            .precision = 0,
            .precision_specified = false,
            .length = PRINTF_LEN_DEFAULT,
            .specifier = '\0'
        };

        bool parsing_flags = true;
        while (parsing_flags) {
            switch (*ptr) {
                case '-': fmt.left_adjust = true; ++ptr; break;
                case '+': fmt.show_sign = true; ++ptr; break;
                case ' ': fmt.space_sign = true; ++ptr; break;
                case '#': fmt.alternate_form = true; ++ptr; break;
                case '0': fmt.zero_pad = true; ++ptr; break;
                default: parsing_flags = false; break;
            }
        }

        if (*ptr == '*') {
            fmt.width = va_arg(args, int);
            if (fmt.width < 0) {
                fmt.left_adjust = true;
                fmt.width = -fmt.width;
            }
            ++ptr;
        } else if (printf_is_digit(*ptr)) {
            fmt.width = printf_parse_number(&ptr);
        }

        if (*ptr == '.') {
            ++ptr;
            fmt.precision_specified = true;
            if (*ptr == '*') {
                fmt.precision = va_arg(args, int);
                if (fmt.precision < 0) {
                    fmt.precision_specified = false;
                }
                ++ptr;
            } else if (printf_is_digit(*ptr)) {
                fmt.precision = printf_parse_number(&ptr);
            } else {
                fmt.precision = 0;
            }
        }

        if (*ptr == 'h') {
            ++ptr;
            if (*ptr == 'h') {
                fmt.length = PRINTF_LEN_HH;
                ++ptr;
            } else {
                fmt.length = PRINTF_LEN_H;
            }
        } else if (*ptr == 'l') {
            ++ptr;
            if (*ptr == 'l') {
                fmt.length = PRINTF_LEN_LL;
                ++ptr;
            } else {
                fmt.length = PRINTF_LEN_L;
            }
        } else if (*ptr == 'L') {
            fmt.length = PRINTF_LEN_CAP_L;
            ++ptr;
        } else if (*ptr == 'z') {
            fmt.length = PRINTF_LEN_Z;
            ++ptr;
        } else if (*ptr == 't') {
            fmt.length = PRINTF_LEN_T;
            ++ptr;
        } else if (*ptr == 'j') {
            fmt.length = PRINTF_LEN_J;
            ++ptr;
        }

        fmt.specifier = *ptr ? *ptr : '\0';
        if (*ptr != '\0') {
            ++ptr;
        }

        switch (fmt.specifier) {
            case 'd':
            case 'i': {
                long long value = printf_get_signed_arg(&args, fmt.length);
                bool negative = value < 0;
                unsigned long long abs_value = negative ? (unsigned long long)(-value) : (unsigned long long)value;
                printf_format_integer(&chunk, &fmt, abs_value, negative, 10, false, false, true);
                break;
            }
            case 'u': {
                unsigned long long value = printf_get_unsigned_arg(&args, fmt.length);
                printf_format_integer(&chunk, &fmt, value, false, 10, false, false, false);
                break;
            }
            case 'x':
            case 'X': {
                unsigned long long value = printf_get_unsigned_arg(&args, fmt.length);
                printf_format_integer(&chunk, &fmt, value, false, 16, fmt.specifier == 'X', false, false);
                break;
            }
            case 'o':
            case 'O': {
                unsigned long long value = printf_get_unsigned_arg(&args, fmt.length);
                printf_format_integer(&chunk, &fmt, value, false, 8, false, false, false);
                break;
            }
            case 'b':
            case 'B': {
                unsigned long long value = printf_get_unsigned_arg(&args, fmt.length);
                printf_format_integer(&chunk, &fmt, value, false, 2, fmt.specifier == 'B', false, false);
                break;
            }
            case 'p': {
                uintptr_t value = (uintptr_t)va_arg(args, void *);
                printf_format_integer(&chunk, &fmt, (unsigned long long)value, false, 16, false, true, false);
                break;
            }
            case 'c': {
                char c = (char)va_arg(args, int);
                printf_format_char(&chunk, &fmt, c);
                break;
            }
            case 's': {
                const char *str = va_arg(args, const char *);
                printf_format_string(&chunk, &fmt, str);
                break;
            }
            case 'n': {
                void *out = va_arg(args, void *);
                if (out) {
                    switch (fmt.length) {
                        case PRINTF_LEN_HH:
                            *(signed char *)out = (signed char)chunk.total_written;
                            break;
                        case PRINTF_LEN_H:
                            *(short *)out = (short)chunk.total_written;
                            break;
                        case PRINTF_LEN_L:
                            *(long *)out = (long)chunk.total_written;
                            break;
                        case PRINTF_LEN_LL:
                            *(long long *)out = (long long)chunk.total_written;
                            break;
                        case PRINTF_LEN_Z:
                            *(size_t *)out = (size_t)chunk.total_written;
                            break;
                        case PRINTF_LEN_T:
                            *(ptrdiff_t *)out = (ptrdiff_t)chunk.total_written;
                            break;
                        case PRINTF_LEN_J:
                            *(long long *)out = (long long)chunk.total_written;
                            break;
                        case PRINTF_LEN_DEFAULT:
                        default:
                            *(int *)out = (int)chunk.total_written;
                            break;
                    }
                }
                break;
            }
            case 'f':
            case 'F': {
                double value;
                if (fmt.length == PRINTF_LEN_CAP_L) {
                    long double ld = va_arg(args, long double);
                    value = (double)ld;
                } else {
                    value = va_arg(args, double);
                }
                printf_format_float(&chunk, &fmt, value, fmt.specifier == 'F');
                break;
            }
            case '\0':
                printf_chunk_emit_char(&chunk, '%');
                break;
            default:
                printf_chunk_emit_char(&chunk, '%');
                if (fmt.specifier) {
                    printf_chunk_emit_char(&chunk, fmt.specifier);
                }
                break;
        }
    }

    va_end(args);
    printf_chunk_flush(&chunk);
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


uint32_t get_screen_framebuffer_cols() {
    return fb_console_use() ? fb_console_state.cols : MAX_COLS;
}
uint32_t get_screen_framebuffer_rows() {
    return fb_console_use() ? fb_console_state.rows : MAX_ROWS;
}
