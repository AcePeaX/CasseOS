%ifndef ELF_FORMAT
    [org 0x7C00]
%endif

; Default to 1 sector if NUM_SECTORS is not defined
%ifndef NUM_SECTORS
%define NUM_SECTORS 70
%endif
KERNEL_OFFSET equ 0x0000 ; The same one we used when linking the kernel
KERNEL_SEGMENT equ 0x8000 ; The same one we used when linking the kernel
KERNEL_FULL_MEM equ 0x80000 ; The same one we used when linking the kernel

    mov [BOOT_DRIVE], dl ; Remember that the BIOS sets us the boot drive in 'dl' on boot
    mov bp, 0x9000 ; set the stack
    mov sp, bp

    mov bx, MSG_REAL_MODE
    call print ; This will be written after the BIOS messages
    call print_nl

    call load_kernel
    call switch_to_pm
    jmp $ ; this will actually never be executed

%include "./boot_print.asm"
%include "./boot_print_hex.asm"
%include "./boot_load_disk.asm"
%include "./32bit-gdt.asm"
%include "./32bit-print.asm"
%include "32bit-switch.asm"

[bits 16]
load_kernel:
    mov bx, MSG_LOAD_KERNEL
    call print
    call print_nl

    mov bx, KERNEL_OFFSET ; Read from disk and store in ....
    mov ax, KERNEL_SEGMENT
    mov es, ax            ; 16*KERNEL_SEGMENT+KERNEL_OFFSET
    mov dh, NUM_SECTORS   ; Number of sectors to read
    mov dl, [BOOT_DRIVE]
    call disk_load
    ret

[bits 32]
BEGIN_PM: ; after the switch we will get here
    mov ebx, MSG_PROT_MODE
    call print_string_pm ; Note that this will be written at the top left corner
    call KERNEL_FULL_MEM ; Give control to the kernel
    jmp $


BOOT_DRIVE db 0 ; It is a good idea to store it in memory because 'dl' may get overwritten
MSG_REAL_MODE db "Started in 16-bit real mode", 0
MSG_PROT_MODE db "Loaded 32-bit protected mode", 0
MSG_LOAD_KERNEL db "Loading kernel into memory", 0

; bootsector
times 510-($-$$) db 0
dw 0xaa55