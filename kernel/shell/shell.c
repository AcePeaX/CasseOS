#include <stdbool.h>
#include "shell.h"
#include "command_line.h"
#include "drivers/screen.h"
#include "drivers/keyboard.h"
#include "cpu/type.h"
#include "libc/string.h"

uint8_t cursor=0;
bool end_command = false;
bool start = true;
bool pop = true;

char command[MAX_COMMAND_LENGTH];

extern char key_buffer;

void shell_main_loop(){
    if(start){
        kprint("Welcome to CasseOS Sell!\n>");
        flush_command_line();
        init_command_line(get_cursor_offset()/2);
        start = false;
    }
    if(end_command){
        kprint("\n");
        kprint("Incorrect command: '");
        kprint(command);
        kprint("'");
        kprint("\n>");
        end_command = false;
        flush_command_line();
        init_command_line(get_cursor_offset()/2);
    }
    uint8_t scancode;
    if((scancode = keyboard_buffer_pop())){
        end_command = handle_command_line(scancode,command);
    }
}