#ifndef STRINGS_H
#define STRINGS_H

#include <stdint.h>

void int_to_ascii(int n, char str[]);
void reverse(char s[]);
int strlen(char s[]);
void backspace(char s[]);
void append(char s[], char n);
int strcmp(char s1[], char s2[]);
void hex_to_string(uint64_t value, char* buffer);
void hex_to_string_trimmed(uint64_t value, char* buffer);

#endif
