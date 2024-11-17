#ifndef SCREEN_H
#define SCREEN_H

#define VIDEO_ADDRESS (char *)0xb8000
#define MAX_ROWS 25
#define MAX_COLS 80
#define WHITE_ON_BLACK 0x0f
#define RED_ON_WHITE 0xf4

/* Screen i/o ports */
#define REG_SCREEN_CTRL 0x3d4
#define REG_SCREEN_DATA 0x3d5

#include <stdbool.h>

/* Public kernel API */
void clear_screen();
void kprint_at(char *message, int col, int row);
void kprint(char *message);
void kprint_backspace();

void set_auto_cursor(bool auto_cursor);
int get_cursor_offset();
void set_cursor_offset(int offset);
int get_vga_offset_row(int offset);
int get_vga_offset_col(int offset);

#endif