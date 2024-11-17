#include "keyboard.h"
#include "../cpu/ports.h"
#include "../cpu/isr.h"
#include "screen.h"
#include "../libc/string.h"
#include "../libc/function.h"
#include <stdint.h>


#define KEY_BUFFER_SIZE 256
char key_buffer[KEY_BUFFER_SIZE];
uint8_t key_buffer_head = 0;
uint8_t key_buffer_tail = 0;

KeyState key_state = {0};

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

const char sc_ascii_azerti[] = { '?', '?', '&', 0x1A /*é*/, '"', '\'', '(', '-',     
    0x1A /*è*/, '_', 0x1A /*ç*/, 0x1A/*à*/, ')', '=', '?', '?', 'a', 'z', 'e', 'r', 't', 'y', 
        'u', 'i', 'o', 'p', '^', '$', '?', '?', 'q', 's', 'd', 'f', 'g', 
        'h', 'j', 'k', 'l', 'm', '?', '*', '?', '*', 'w', 'x', 'c', 'v', 
        'b', 'n', ',', ',', ';', ':', '!', '?', '?', ' '};

const char sc_ascii_azerti_cap[] = { '?', '?', '1', '2', '3', '4', '5', '6',     
    '7', '8', '9', '0', 0x1A/*°*/, '+', '?', '?', 'A', 'Z', 'E', 'R', 'T', 'Y', 
        'U', 'I', 'O', 'P', 0x1A/*¨*/, 0x1A /*£*/, '?', '?', 'Q', 'S', 'D', 'F', 'G', 
        'H', 'J', 'K', 'L', 'M', '?', '*', '?', '*', 'W', 'X', 'C', 'V', 
        'B', 'N', '?', '?', '.', '/', 0x1A/*§*/, '?', '?', ' '};

const char* sc_ascii = sc_ascii_azerti;
const char* sc_ascii_cap = sc_ascii_azerti_cap;


uint8_t keyboard_get_ascii_from_scancode(uint8_t scancode);
void keyboard_buffer_push(char scancode);

static void keyboard_callback(registers_t *regs) {
    /* The PIC leaves us the scancode in port 0x60 */
    uint8_t scancode = port_byte_in(0x60);

    keyboard_buffer_push(scancode);
    UNUSED(regs);
}


uint8_t keyboard_get_ascii_from_scancode(uint8_t scancode){
    uint8_t rawcode = scancode & 0b01111111;
    uint8_t up = (scancode >> 7) & 0b1;
    uint8_t cap = keyboard_cap_lock ^ keyboard_shift;
    
    if(scancode==CAP_LOCK){
        keyboard_cap_lock=1-keyboard_cap_lock;
        return 0;
    }
    if(rawcode==CTRL){
        if(up){
            keyboard_ctrl=0;
        }
        else{
            keyboard_ctrl=1;
        }
        return 0;
    }
    if(rawcode==SHIFT){
        if(up){
            keyboard_shift=0;
        }
        else{
            keyboard_shift=1;
        }
        return 0;
    }
    
    if (scancode == BACKSPACE) {
        return BACKSPACE;
    } else if (scancode == ENTER) {
        return ENTER;
    } 
    else if(!up) {
        char letter;
        if(cap){
            letter = sc_ascii_cap[(int)scancode];
        } else {
            letter = sc_ascii[(int)scancode];
        }
        return letter;
    }
    return 0;
}

void keyboard_buffer_push(char scancode) {
    int next = (key_buffer_head + 1) % KEY_BUFFER_SIZE;
    if (next != key_buffer_tail) {  // Only add if buffer is not full
        key_buffer[key_buffer_head] = scancode;
        key_buffer_head = next;
    }
}

char keyboard_buffer_pop(){
    if (key_buffer_head != key_buffer_tail) {
        char scancode = key_buffer[key_buffer_tail];
        key_buffer_tail = (key_buffer_tail + 1) % KEY_BUFFER_SIZE;
        return scancode;
    }
    return 0x00;
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
