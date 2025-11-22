%define KERNEL_LBA_START 1
%define FLOPPY_SECTORS_PER_TRACK 18
%define FLOPPY_HEADS 2
%define FLOPPY_SECTORS_PER_CYL (FLOPPY_SECTORS_PER_TRACK * FLOPPY_HEADS)

; load 'dh' sectors from drive 'dl' into ES:BX
disk_load:
    pusha

    xor cx, cx
    mov cl, dh                ; total sectors requested
    mov dl, [BOOT_DRIVE]
    cmp dl, 0x80
    jb .chs_path

    mov ax, cx
    mov [dap_count], ax
    mov [dap_offset], bx
    mov [dap_segment], es
    mov word [dap_lba], KERNEL_LBA_START
    call read_dap
    jmp .done

.chs_path:
    mov bp, cx
    mov si, KERNEL_LBA_START
    mov di, bx

.chs_loop:
    cmp bp, 0
    jz .chs_exit

    mov ax, si
    xor dx, dx
    mov bx, FLOPPY_SECTORS_PER_CYL
    div bx                             ; AX=cylinder, DX=remainder
    push ax
    mov ch, al
    mov ax, dx
    xor dx, dx
    mov bx, FLOPPY_SECTORS_PER_TRACK
    div bx                             ; AX=head, DX=sector index
    mov dh, al
    mov cl, dl
    inc cl
    pop ax
    mov bl, ah
    and bl, 0x03
    shl bl, 6
    or cl, bl
    mov dl, [BOOT_DRIVE]
    mov bx, di
    mov ah, 0x02
    mov al, 0x01
    int 0x13
    jc disk_error

    add di, 512
    inc si
    dec bp
    jmp .chs_loop

.chs_exit:
    jmp .done

.done:
    popa
    ret

read_dap:
    mov si, dap
    mov dl, [BOOT_DRIVE]
    mov ah, 0x42
    int 0x13
    jc disk_error
    ret

disk_error:
    mov bx, DISK_ERROR
    call print
    call print_nl
    mov dh, ah
    call print_hex
disk_loop:
    jmp disk_loop

dap:
    db 16
    db 0
dap_count:
    dw 0
dap_offset:
    dw 0
dap_segment:
    dw 0
dap_lba:
    dq 0

DISK_ERROR: db "DE", 0
