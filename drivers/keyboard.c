#include "keyboard.h"
#include "../cpu/ports.h"
#include "../cpu/isr.h"
#include "screen.h"
#include "../libc/string.h"
#include "../libc/function.h"
#include "../kernel/kernel.h"
#include <stdint.h>

#define BACKSPACE 0x0E
#define ENTER 0x1C
#define CTRL 0x1D
#define SHIFT 0x2A
#define CAP_LOCK 0x3A

char key_buffer[256];

uint8_t keyboard_auto_display = 1;
uint8_t keyboard_ctrl = 0;
uint8_t keyboard_shift = 0;
uint8_t keyboard_cap_lock = 0;

#define SC_MAX 57
/*const char *sc_name[] = { "ERROR", "Esc", "1", "2", "3", "4", "5", "6", 
    "7", "8", "9", "0", "-", "=", "Backspace", "Tab", "Q", "W", "E", 
        "R", "T", "Y", "U", "I", "O", "P", "[", "]", "Enter", "Lctrl", 
        "A", "S", "D", "F", "G", "H", "J", "K", "L", ";", "'", "`", 
        "LShift", "\\", "Z", "X", "C", "V", "B", "N", "M", ",", ".", 
        "/", "RShift", "Keypad *", "LAlt", "Spacebar"};*/
const char sc_ascii_qwerti[] = { '?', '?', '1', '2', '3', '4', '5', '6',     
    '7', '8', '9', '0', '-', '=', '?', '?', 'q', 'w', 'e', 'r', 't', 'y', 
        'u', 'i', 'o', 'p', '[', ']', '?', '?', 'a', 's', 'd', 'f', 'g', 
        'h', 'j', 'k', 'l', ';', '\'', '`', '?', '\\', 'z', 'x', 'c', 'v', 
        'b', 'n', 'm', ',', '.', '/', '?', '?', '?', ' '};
const char sc_ascii_qwerti_cap[] = { '?', '?', '1', '2', '3', '4', '5', '6',     
    '7', '8', '9', '0', '-', '=', '?', '?', 'Q', 'W', 'E', 'R', 'T', 'Y', 
        'U', 'I', 'O', 'P', '[', ']', '?', '?', 'A', 'S', 'D', 'F', 'G', 
        'H', 'J', 'K', 'L', ';', '\'', '`', '?', '\\', 'Z', 'X', 'C', 'V', 
        'B', 'N', 'M', ',', '.', '/', '?', '?', '?', ' '};

const char sc_ascii_azerti[] = { '?', '?', '&', 'é', '"', '\'', '(', '-',     
    'è', '_', 'ç', 'à', ')', '=', '?', '?', 'a', 'z', 'e', 'r', 't', 'y', 
        'u', 'i', 'o', 'p', '^', '$', '?', '?', 'q', 's', 'd', 'f', 'g', 
        'h', 'j', 'k', 'l', 'm', '?', '*', '?', '*', 'w', 'x', 'c', 'v', 
        'b', 'n', ',', ',', ';', ':', '!', '?', '?', ' '};

const char sc_ascii_azerti_cap[] = { '?', '?', '1', '2', '3', '4', '5', '6',     
    '7', '8', '9', '0', '°', '+', '?', '?', 'A', 'Z', 'E', 'R', 'T', 'Y', 
        'U', 'I', 'O', 'P', '¨', '£', '?', '?', 'Q', 'S', 'D', 'F', 'G', 
        'H', 'J', 'K', 'L', 'M', '?', '*', '?', '*', 'W', 'X', 'C', 'V', 
        'B', 'N', '?', '?', '.', '/', '§', '?', '?', ' '};

const char* sc_ascii = sc_ascii_azerti;
const char* sc_ascii_cap = sc_ascii_azerti_cap;

static void keyboard_callback(registers_t *regs) {
    /* The PIC leaves us the scancode in port 0x60 */
    uint8_t scancode = port_byte_in(0x60);

    uint8_t rawcode = scancode & 0b01111111;
    uint8_t up = (scancode >> 7) & 0b1;
    uint8_t cap = keyboard_cap_lock ^ keyboard_shift;
    
    if(scancode==CAP_LOCK){
        keyboard_cap_lock=1-keyboard_cap_lock;
        return;
    }
    if (rawcode > SC_MAX) {
        return;
    };
    if(rawcode==CTRL){
        if(up){
            keyboard_ctrl=0;
        }
        else{
            keyboard_ctrl=1;
        }
        return;
    }
    if(rawcode==SHIFT){
        if(up){
            keyboard_shift=0;
        }
        else{
            keyboard_shift=1;
        }
        return;
    }
    
    if (scancode == BACKSPACE) {
        backspace(key_buffer);
        if(keyboard_auto_display==1){
            kprint_backspace();
        }
    } else if (scancode == ENTER) {
        if(keyboard_auto_display==1){kprint("\n");}
        user_input(key_buffer); /* kernel-controlled function */
        key_buffer[0] = '\0';
    } 
    else if(!up) {
        char letter;
        if(cap){
            letter = sc_ascii_cap[(int)scancode];
        } else {
            letter = sc_ascii[(int)scancode];
        }
        /* Remember that kprint only accepts char[] */
        char str[2] = {letter, '\0'};
        append(key_buffer, letter);
        if(keyboard_auto_display==1){kprint(str);}
    }
    UNUSED(regs);
}

void set_keyboard_type(uint8_t type){
    if(type==0){
        sc_ascii = sc_ascii_qwerti;
        sc_ascii_cap = sc_ascii_qwerti_cap;
    }
    else if(type==1){
        sc_ascii_cap = sc_ascii_azerti_cap;
    }
}

void init_keyboard() {
    register_interrupt_handler(IRQ1, keyboard_callback); 
}
