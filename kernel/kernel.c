#include "../drivers/screen.h"
#include "util.h"

void main() {
    clear_screen();
    kprint("Welcome to CasseOS!");
    /* Fill up the screen */
    int i = 0;
    for (i = 0; i < 24; i++) {
        char str[255];
        int_to_ascii(i, str);
        kprint_at(str, 0, i+1);
        for (int l = 0; l < 32000; l++) {
            for (int j = 0; j < 3200; j++) {
            }
        }
    }
    kprint("\nNow it switches");
}