; boot.asm
[BITS 16]                 ; Set 16-bit mode
ORG 0x7C00                ; Bootloader loads at memory address 0x7C00

    jmp start
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

section .data
    current_line dd 0      ; Define a 32-bit variable initialized to 0

section .text
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

    ; Adjust cursor style
    ; Set cursor start register (blinking cursor)
    mov dx, 0x3D4          ; Address register port
    mov al, 0x0A           ; Select cursor start register (0x0A)
    out dx, al
    inc dx                 ; Data register port (0x3D5)
    mov al, 0x0E           ; Set bits 4-5 for visibility and 0-3 for start scan line (blinking)
    out dx, al

    ; Set cursor end register
    mov dx, 0x3D4          ; Address register port
    mov al, 0x0B           ; Select cursor end register (0x0B)
    out dx, al
    inc dx                 ; Data register port (0x3D5)
    mov al, 0x0F           ; Set bits 0-3 for end scan line
    out dx, al

    ; Display message in protected mode

    mov eax, 160                ; Each line takes 160 bytes (80 chars * 2 bytes per char)
    mov [current_line], eax     

    mov esi, message
    call print_string
    call protected_back_to_line
    mov esi, message
    call print_string
    mov esi, message
    call print_string
    call protected_back_to_line
    mov esi, message
    call print_string
    mov esi, message
    call print_string

    ; Infinite loop to prevent execution beyond intended code
hang:
    jmp hang




protected_back_to_line:
    mov eax, [current_line]
    mov ecx, eax
    mov edx, 0
    mov ebx, 160
    div ebx
    sub ecx, edx
    add ecx, 160
    mov [current_line], ecx

; Print function in protected mode
print_string:
    mov edx, 0xB8000            ; VGA text mode memory address
    mov bh, 0x00                ; Row (not used here)
    mov bl, 0x07                ; Attribute byte (light grey on black)
    mov eax, [current_line]     ; Each line takes 160 bytes (80 chars * 2 bytes per char)
    add edx, eax                ; Move the cursor to the start of the next line
    

next_char:
    lodsb                       ; Load byte from [ESI] into AL, increment ESI
    cmp al, 0
    je done_next_char                     ; If null terminator, exit
    mov [edx], al               ; Write character to video memory
    inc edx
    mov [edx], bl               ; Write attribute byte
    inc edx
    jmp next_char

done_next_char:
    mov ecx, 0
    mov cx, dx
    shl cx, 3
    shr cx, 3
    mov [current_line], ecx
    shr cx, 1
    ; Set the cursor location in VGA
    mov dx, 0x3D4          ; Address register port
    mov al, 0x0F           ; Select cursor low byte register (0x0F)
    out dx, al
    inc dx                 ; Data register port (0x3D5)
    mov al, cl
    out dx, al

    mov dx, 0x3D4          ; Address register port
    mov al, 0x0E           ; Select cursor high byte register (0x0E)
    out dx, al
    inc dx                 ; Data register port (0x3D5)
    mov al, ch
    out dx, al
    ret

message db "start......", 0 ; Null-terminated string

times 510-($-$$) db 0           ; Fill remaining space with zeros
dw 0xAA55                       ; Boot signature
