; boot.asm
BITS 16              ; Set 16-bit mode
ORG 0x7C00           ; Bootloader loads at memory address 0x7C00

start:
    mov si, msg
    mov ah, 0x0E       ; BIOS teletype function to display character
print_char:
    lodsb
    cmp al, 0
    je hang

    int 0x10           ; Call interrupt 0x10 to print character
    jmp print_char

hang:
    jmp hang           ; Loop forever


msg db "Booting...", 0 ; Define the message with a null terminator

times 510-($-$$) db 0  ; Fill remaining space with zeros
dw 0xAA55              ; Boot signature
