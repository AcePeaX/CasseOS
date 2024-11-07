#include "command_line.h"
#include "drivers/keyboard.h"
#include "drivers/screen.h"
#include "libc/string.h"

int cmd_start_offset = 0;
uint16_t cmd_cursor = 0;
uint16_t cmd_len = 0;

void init_command_line(int start_offset){
    cmd_start_offset = start_offset;
}

bool handle_command_line(uint8_t scancode, char* command){
    /*char s[10];
    int_to_ascii(scancode, s);
    kprint(s);
    kprint("\n");
    return true;*/
    if(scancode==ENTER){
        return true;
    }
    else if(scancode==BACKSPACE){
        if(cmd_cursor>0){
            kprint_backspace();
            if(cmd_cursor!=cmd_len){
                for(int i=cmd_cursor-1; i<cmd_len+1; i++){
                    command[i] = command[i+1];
                }
                int offset = (cmd_cursor+cmd_start_offset)*2-1;
                int row = get_vga_offset_row(offset);
                int col = get_vga_offset_col(offset);
                command[cmd_len-1] = ' ';
                kprint_at(command+cmd_cursor-1,col,row);
                command[cmd_len-1] = 0;
            }
            cmd_cursor--;
            cmd_len--;
        }
    } else if(scancode==DEL){
        if(cmd_cursor<cmd_len){
            for(int i=cmd_cursor; i<cmd_len+1; i++){
                command[i] = command[i+1];
            }
            int offset = (cmd_cursor+cmd_start_offset)*2-1;
            int row = get_vga_offset_row(offset);
            int col = get_vga_offset_col(offset);
            command[cmd_len-1] = ' ';
            kprint_at(command+cmd_cursor-1,col,row);
            command[cmd_len-1] = 0;
            cmd_len--;
        }
    } else if(scancode==ARROW_LEFT){
        if(cmd_cursor>0){cmd_cursor--;}
    } else if(scancode==ARROW_RIGHT){
        cmd_cursor++;
        if(cmd_cursor>cmd_len){cmd_cursor=cmd_len;}
    }
    else{
        if(cmd_len==MAX_COMMAND_LENGTH){
            return false;
        }
        char c = keyboard_get_ascii_from_scancode(scancode);
        if(c!=0){
            if(cmd_cursor!=cmd_len){
                for(int i=cmd_len-1; i>=cmd_cursor; i--){
                    command[i+1] = command[i];
                }
            }
            command[cmd_len+1] = 0;
            command[cmd_cursor] = c;
            int offset = (cmd_cursor+cmd_start_offset)*2;
            int row = get_vga_offset_row(offset);
            int col = get_vga_offset_col(offset);
            if(cmd_cursor!=cmd_len){
                kprint_at(command+cmd_cursor,col,row);
            }
            else{
                char c_str[2] = "";
                c_str[0] = c;
                kprint_at(c_str, col, row);
            }
            cmd_cursor++;
            cmd_len++;
        }
    }
    set_cursor_offset((cmd_cursor+cmd_start_offset)*2);
    return false;
}
void flush_command_line(){
    cmd_cursor = 0;
    cmd_len = 0;
}