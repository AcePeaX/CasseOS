#include "string.h"

/**
 * K&R implementation
 */
void int_to_ascii(int n, char str[]) {
    int i, sign;
    if ((sign = n) < 0) n = -n;
    i = 0;
    do {
        str[i++] = n % 10 + '0';
    } while ((n /= 10) > 0);

    if (sign < 0) str[i++] = '-';
    str[i] = '\0';

    reverse(str);
}

void uint_to_ascii(unsigned int value, char *str) {
    char temp[16];
    int i = 0;

    // Handle 0 explicitly
    if (value == 0) {
        str[0] = '0';
        str[1] = '\0';
        return;
    }

    // Convert number to string in reverse
    while (value > 0) {
        temp[i++] = '0' + (value % 10u);
        value /= 10u;
    }

    // Reverse the string
    int j = 0;
    while (i > 0) {
        str[j++] = temp[--i];
    }
    str[j] = '\0';
}


void hex_to_string(uint64_t value, char* buffer) {
    const char* hex_digits = "0123456789ABCDEF";
    int i;
    for (i = 15; i >= 0; --i) {
        buffer[i] = hex_digits[value & 0xF];
        value >>= 4; // Shift the value 4 bits to the right
    }
    buffer[16] = '\0'; // Null-terminate the string
}

void hex_to_string_trimmed(uint64_t value, char* buffer) {
    const char* hex_digits = "0123456789ABCDEF";
    int i = 15; // Start from the least significant digit
    int start = 0;

    // Fill digits in reverse
    while (value > 0 || i == 15) { // Always output at least one zero
        buffer[i--] = hex_digits[value & 0xF];
        value >>= 4;
    }

    // Move the digits to the beginning of the buffer
    for (start = 0, ++i; i < 16; ++i, ++start) {
        buffer[start] = buffer[i];
    }
    buffer[start] = '\0';
}


/* K&R */
void reverse(char s[]) {
    int c, i, j;
    for (i = 0, j = strlen(s)-1; i < j; i++, j--) {
        c = s[i];
        s[i] = s[j];
        s[j] = c;
    }
}

/* K&R */
int strlen(char s[]) {
    int i = 0;
    while (s[i] != '\0') ++i;
    return i;
}

void append(char s[], char n) {
    int len = strlen(s);
    s[len] = n;
    s[len+1] = '\0';
}

void backspace(char s[]) {
    int len = strlen(s);
    s[len-1] = '\0';
}

/* K&R 
 * Returns <0 if s1<s2, 0 if s1==s2, >0 if s1>s2 */
int strcmp(char s1[], char s2[]) {
    int i;
    for (i = 0; s1[i] == s2[i]; i++) {
        if (s1[i] == '\0') return 0;
    }
    return s1[i] - s2[i];
}

