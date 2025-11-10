#include <stdarg.h>
#include "screen.h"
#include "cpu/ports.h"
#include "libc/string.h"
#include "libc/mem.h"


/* Declaration of private functions */
int get_cursor_offset();
void set_cursor_offset(int offset);
int print_char(char c, int col, int row, char attr);
int get_offset(int col, int row);
int get_vga_offset_row(int offset);
int get_vga_offset_col(int offset);

bool screen_auto_cursor = true;

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
    kprint_at(message, -1, -1);
}

void kprint_backspace() {
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
            memory_copy((uint8_t*)(get_offset(0, i) + VIDEO_ADDRESS),
                        (uint8_t*)(get_offset(0, i-1) + VIDEO_ADDRESS),
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
    /* Similar to get_cursor_offset, but instead of reading we write data */
    offset /= 2;
    port_byte_out(REG_SCREEN_CTRL, 14);
    port_byte_out(REG_SCREEN_DATA, (unsigned char)(offset >> 8));
    port_byte_out(REG_SCREEN_CTRL, 15);
    port_byte_out(REG_SCREEN_DATA, (unsigned char)(offset & 0xff));
}

void clear_screen() {
    int screen_size = MAX_COLS * MAX_ROWS;
    int i;
    char *screen = VIDEO_ADDRESS;

    for (i = 0; i < screen_size; i++) {
        screen[i*2] = ' ';
        screen[i*2+1] = WHITE_ON_BLACK;
    }
    set_cursor_offset(get_offset(0, 0));
}


int get_offset(int col, int row) { return 2 * (row * MAX_COLS + col); }
int get_vga_offset_row(int offset) { return offset / (2 * MAX_COLS); }
int get_vga_offset_col(int offset) { return (offset - (get_vga_offset_row(offset)*2*MAX_COLS))/2; }


void printf(const char *format, ...) {
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
