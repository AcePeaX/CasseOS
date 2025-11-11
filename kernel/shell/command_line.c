#include "command_line.h"
#include "drivers/keyboard/keyboard.h"
#include "drivers/screen.h"
#include "libc/string.h"

static int cmd_start_offset = 0;
static uint16_t cmd_cursor = 0;
static uint16_t cmd_len = 0;

static void refresh_command_visual(const char *command, uint16_t previous_len)
{
    int base_offset = cmd_start_offset * 2;
    int row = get_vga_offset_row(base_offset);
    int col = get_vga_offset_col(base_offset);
    kprint_at(command, col, row);

    if (previous_len > cmd_len) {
        int blank_offset = (cmd_start_offset + cmd_len) * 2;
        for (uint16_t i = cmd_len; i < previous_len; ++i) {
            int blank_row = get_vga_offset_row(blank_offset);
            int blank_col = get_vga_offset_col(blank_offset);
            kprint_at(" ", blank_col, blank_row);
            blank_offset += 2;
        }
    }

    set_cursor_offset((cmd_start_offset + cmd_cursor) * 2);
}

void init_command_line(int start_offset){
    cmd_start_offset = start_offset;
}

static void delete_at(char* command, uint16_t position)
{
    for (uint16_t i = position; i < cmd_len; ++i) {
        command[i] = command[i + 1];
    }
}

static void insert_at(char *command, uint16_t position, char c)
{
    for (int i = cmd_len; i > position; --i) {
        command[i] = command[i - 1];
    }
    command[position] = c;
}

bool handle_command_line(keycode_t code, char* command){
    if (!command) return false;

    if (code == KC_ENTER) {
        command[cmd_len] = '\0';
        return true;
    }

    uint16_t prev_len = cmd_len;

    if (code == KC_BACKSPACE) {
        if (cmd_cursor > 0) {
            cmd_cursor--;
            delete_at(command, cmd_cursor);
            if (cmd_len > 0) cmd_len--;
            refresh_command_visual(command, prev_len);
        }
        return false;
    }

    if (code == KC_DELETE) {
        if (cmd_cursor < cmd_len) {
            delete_at(command, cmd_cursor);
            if (cmd_len > 0) cmd_len--;
            refresh_command_visual(command, prev_len);
        }
        return false;
    }

    if (code == KC_LEFT) {
        if (cmd_cursor > 0) cmd_cursor--;
        set_cursor_offset((cmd_start_offset + cmd_cursor) * 2);
        return false;
    }

    if (code == KC_RIGHT) {
        if (cmd_cursor < cmd_len) cmd_cursor++;
        set_cursor_offset((cmd_start_offset + cmd_cursor) * 2);
        return false;
    }

    if (code == KC_HOME) {
        cmd_cursor = 0;
        set_cursor_offset((cmd_start_offset + cmd_cursor) * 2);
        return false;
    }

    if (code == KC_END) {
        cmd_cursor = cmd_len;
        set_cursor_offset((cmd_start_offset + cmd_cursor) * 2);
        return false;
    }

    if (code >= 0x20 && code < 0x7F) {
        if (cmd_len >= (MAX_COMMAND_LENGTH - 1)) {
            return false;
        }
        insert_at(command, cmd_cursor, (char)code);
        cmd_len++;
        command[cmd_len] = '\0';
        cmd_cursor++;
        refresh_command_visual(command, prev_len);
    }

    return false;
}

void flush_command_line(char* command){
    cmd_cursor = 0;
    cmd_len = 0;
    if (command) {
        command[0] = '\0';
    }
    set_cursor_offset(cmd_start_offset * 2);
}
