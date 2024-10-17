; boot.asm
BITS 16                  ; Set 16-bit mode
ORG 0x7C00               ; Bootloader loads at memory address 0x7C00

; GDT Setup
gdt_start:
    ; Null descriptor (8 bytes)
    dq 0

    ; Code segment descriptor (8 bytes)
    dw 0xFFFF          ; Limit (15:0)
    dw 0x0000          ; Base (15:0)
    db 0x00            ; Base (23:16)
    db 10011010b       ; Access byte: Present, Privilege=0, Code segment, Executable, Readable
    db 11001111b       ; Granularity, 32-bit segment
    db 0x00            ; Base (31:24)

    ; Data segment descriptor (8 bytes)
    dw 0xFFFF          ; Limit (15:0)
    dw 0x0000          ; Base (15:0)
    db 0x00            ; Base (23:16)
    db 10010010b       ; Access byte: Present, Privilege=0, Data segment, Writable
    db 11001111b       ; Granularity, 32-bit segment
    db 0x00            ; Base (31:24)

gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1   ; Size of GDT
    dd gdt_start                 ; Address of GDT

; Bootloader start
start:
    mov si, msg
    mov ah, 0x0E       ; BIOS teletype function to display character
print_char:
    lodsb
    cmp al, 0
    je next

    int 0x10           ; Call interrupt 0x10 to print character
    jmp print_char

next:
    cli                          ; Clear interrupts
    lgdt [gdt_descriptor]        ; Load GDT
    mov eax, cr0
    or eax, 1                    ; Set Protected Mode Enable bit
    mov cr0, eax
    jmp next                     ; Loop forever
    jmp 0x08:protected_mode      ; Far jump to code segment at 0x08


[BITS 32]
protected_mode:
    ; Now we are in protected mode
    mov ax, 0x10                 ; Data segment selector
    mov ds, ax                   ; Set data segment register
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax                   ; Set stack segment

    ; Display a simple message in protected mode (using 32-bit code)
    mov esi, message
    call print_string

; Print function (Assuming VGA text mode)
print_string:
    mov ah, 0x0E                 ; BIOS teletype function (still using BIOS in 16-bit mode)
next_char:
    lodsb                        ; Load byte from [ESI] into AL and increment ESI
    cmp al, 0
    je done                      ; If byte is zero (end of string), return
    int 0x10                     ; Print character in AL
    jmp next_char
done:
    ret


msg db "Booting...", 0 ; Define the message with a null terminator
message db "...", 0  ; Null-terminated string

times 510-($-$$) db 0  ; Fill remaining space with zeros
dw 0xAA55              ; Boot signature
