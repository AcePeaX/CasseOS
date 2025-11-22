#include <stdbool.h>
#include "shell.h"
#include "command_line.h"
#include "drivers/screen.h"
#include "drivers/keyboard/keyboard.h"
#include "cpu/type.h"
#include "libc/string.h"
#include "drivers/usb/usb.h"
#include "drivers/block/ahci.h"

uint8_t cursor=0;
bool end_command = false;
bool start = true;
bool pop = true;

char command[MAX_COMMAND_LENGTH];

extern char key_buffer;

void shell_main_loop(){
    if(start){
        kprint("Welcome to CasseOS Shell!\n>");
        init_command_line(get_cursor_offset()/2);
        flush_command_line(command);
        start = false;
    }
    if(end_command){
        kprint("\n");

        if(strcmp(command, "usb_scan")==0){
            kprint("Executing the scan...\n");
            pci_scan_for_usb_controllers();
        }
        else if(strcmp(command, "ahci_info")==0){
            kprint("AHCI controller summary:\n");
            ahci_print_summary();
        }
        else{
            kprint("Incorrect command: '");
            kprint(command);
            kprint("'\n");
        }
        kprint(">");
        end_command = false;
        init_command_line(get_cursor_offset()/2);
        flush_command_line(command);
    }
    key_event_t ev;
    //printf("Check\n");
    while (kbd_read_event(&ev)) {
        if (ev.type == KEY_EV_PRESS) {
            end_command = handle_command_line(ev.code,command);
        }
    }
}
