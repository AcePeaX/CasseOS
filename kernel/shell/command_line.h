#ifndef CMD_LINE_H
#define CMD_LINE_H
#include "cpu/type.h"

#define MAX_COMMAND_LENGTH 2000

void init_command_line(int start_offset);
bool handle_command_line(uint8_t scancode, char* command);
void flush_command_line();

#endif