; bootloader.asm - First-Stage Bootloader (512 bytes)
[BITS 16]
ORG 0x7C00

start:
    cli                     ; Disable interrupts
    xor ax, ax
    mov ds, ax              ; Data segment to 0
    mov es, ax              ; Extra segment to 0
    mov ss, ax              ; Stack segment to 0
    mov sp, 0x7C00          ; Stack pointer

    mov [boot_drive], dl    ; Store the boot drive number from DL

    mov ax, second_stage_segment
    mov es, ax

    ; Load the second-stage bootloader
    mov bx, 0x0000          ; Offset 0x0000
    mov dh, num_sectors     ; Number of sectors to read
    mov dl, [boot_drive]    ; Drive number (from BIOS)
    call disk_load

    ; Jump to second-stage bootloader
    mov si, booting_msg
    call print_string
    jmp second_stage_segment:0x0000

disk_load:
    ; BIOS interrupt 13h to read sectors
    mov ah, 0x02            ; Function 02h - Read sectors
    mov al, dh              ; Number of sectors to read
    mov ch, 0x00            ; Cylinder 0
    mov cl, 0x02            ; Sector 2 (first sector after bootloader)
    mov dh, 0x00            ; Head 0
    int 0x13                ; Call BIOS
    jc disk_error           ; Jump if carry flag is set (error)
    ret

disk_error:
    ; Handle disk read error
    mov si, disk_error_msg
    call print_string
    hlt

print_string:
    mov ah, 0x0E            ; Teletype output
.print_char:
    lodsb                   ; Load byte from DS:SI into AL
    cmp al, 0
    je .done
    int 0x10                ; BIOS video interrupt
    jmp .print_char
.done:
    ret

boot_drive db 0             ; BIOS sets the boot drive number here
second_stage_segment dw 0x8000  ; Load at 0x0800:0x0000
num_sectors db 1            ; Adjust based on the size of your second-stage bootloader

disk_error_msg db "Disk Read Error", 0
booting_msg db "Booting...", 0

times 510 - ($ - $$) db 0   ; Fill the rest with zeros
dw 0xAA55                   ; Boot signature
