#include "../drivers/screen.h"
#include "util.h"
#include "../cpu/isr.h"
#include "../cpu/idt.h"

void main() {
    isr_install();
    clear_screen();
    __asm__ __volatile__("int $2");
    __asm__ __volatile__("int $3");
    kprint("Welcome to CasseOS!\n");
    // Wait a bit
    for (int l = 0; l < 32000; l++) {
        for (int j = 0; j < 8200; j++) {
        }
    }
    /* Fill up the screen */
    int i = 0;
    for (i = 0; i < 201; i++) {
        char str[255];
        int_to_ascii(i, str);
        kprint(str);
        kprint("\n");
        int wait_iter = 150+7200/(i+1);
        for (int l = 0; l < 32000; l++) {
            for (int j = 0; j < wait_iter; j++) {
            }
        }
    }
    kprint("\nSeems like working");
}