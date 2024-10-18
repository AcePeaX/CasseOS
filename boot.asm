; boot.asm
[BITS 16]                 ; Set 16-bit mode
ORG 0x7C00                ; Bootloader loads at memory address 0x7C00

; GDT Setup
gdt_start:
    ; Null descriptor (8 bytes)
    dq 0

    ; Code segment descriptor (8 bytes)
    dw 0xFFFF           ; Limit (15:0)
    dw 0x0000           ; Base (15:0)
    db 0x00             ; Base (23:16)
    db 10011010b        ; Access byte: Code segment
    db 11001111b        ; Granularity, 32-bit segment
    db 0x00             ; Base (31:24)

    ; Data segment descriptor (8 bytes)
    dw 0xFFFF           ; Limit (15:0)
    dw 0x0000           ; Base (15:0)
    db 0x00             ; Base (23:16)
    db 10010010b        ; Access byte: Data segment
    db 11001111b        ; Granularity, 32-bit segment
    db 0x00             ; Base (31:24)

gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1  ; Size of GDT
    dd gdt_start                ; Address of GDT

; Bootloader start
start:
    ; Display initial message
    mov si, msg
    mov ah, 0x0E       ; BIOS teletype function to display character
print_char:
    lodsb
    cmp al, 0
    je after_print
    int 0x10           ; Call interrupt 0x10 to print character
    jmp print_char

after_print:
    ; Enable A20 line
    in al, 0x92
    or al, 00000010b
    out 0x92, al

    cli                         ; Disable interrupts

    ; Load GDT
    lgdt [gdt_descriptor]       ; Load GDT

    ; Enter protected mode
    mov eax, cr0
    or eax, 1                   ; Set PE bit
    mov cr0, eax

    ; Far jump to flush prefetch queue and load CS
    jmp 0x08:protected_mode     ; Far jump to code segment at 0x08

msg db "Booting...", 0          ; Define the message with a null terminator

[BITS 32]
protected_mode:
    ; Now in protected mode
    ; Update segment registers
    mov ax, 0x10                ; Data segment selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Initialize stack pointer
    mov esp, 0x90000            ; Set stack pointer (adjust as needed)

    ; Display message in protected mode
    mov esi, message
    call print_string

    ; Infinite loop to prevent execution beyond intended code
hang:
    jmp hang

; Print function in protected mode
print_string:
    mov edx, 0xB8000            ; VGA text mode memory address
    mov bh, 0x00                ; Row (not used here)
    mov bl, 0x07                ; Attribute byte (light grey on black)

next_char:
    lodsb                       ; Load byte from [ESI] into AL, increment ESI
    cmp al, 0
    je done                     ; If null terminator, exit
    mov [edx], al               ; Write character to video memory
    inc edx
    mov [edx], bl               ; Write attribute byte
    inc edx
    jmp next_char

done:
    ret

message db "Protected Mode!", 0 ; Null-terminated string

times 510-($-$$) db 0           ; Fill remaining space with zeros
dw 0xAA55                       ; Boot signature
