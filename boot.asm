; boot.asm
BITS 16              ; Set 16-bit mode
ORG 0x7C00           ; Bootloader loads at memory address 0x7C00

start:
    mov ah, 0x0E       ; BIOS teletype function to display character
    mov al, 'H'
    int 0x10           ; Call interrupt 0x10 to print character
    mov al, 'i'
    int 0x10

hang:
    jmp hang           ; Loop forever

times 510-($-$$) db 0  ; Fill remaining space with zeros
dw 0xAA55              ; Boot signature
