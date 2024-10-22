; second_stage.asm - Second-Stage Bootloader
[BITS 16]
ORG 0x0000  ; Assuming loaded at offset 0x0000 in segment 0x8000

start:
    cli                 ; Disable interrupts
    cld                 ; Clear direction flag

    ; Enable A20 Line
    in al, 0x92
    or al, 00000010b
    out 0x92, al

    ; Display message in real mode
    mov si, real_mode_msg
    call print_string

    ; Set up the GDT
    lgdt [gdt_descriptor]

    ; Enter Protected Mode
    mov eax, cr0
    or eax, 1           ; Set PE bit
    mov cr0, eax
    jmp 0x08:protected_mode   ; Far jump to protected mode code segment

; ----------------------------
; GDT Setup
; ----------------------------
gdt_start:
    ; Null Descriptor
    dq 0

    ; Code Segment Descriptor (Protected Mode, 4 GB, base 0)
    dw 0xFFFF           ; Limit Low
    dw 0x0000           ; Base Low
    db 0x00             ; Base Middle
    db 10011010b        ; Access Byte: Code Segment, Executable, Readable
    db 11001111b        ; Flags: Granularity, 32-bit
    db 0x00             ; Base High

    ; Data Segment Descriptor (Protected Mode, 4 GB, base 0)
    dw 0xFFFF           ; Limit Low
    dw 0x0000           ; Base Low
    db 0x00             ; Base Middle
    db 10010010b        ; Access Byte: Data Segment, Writable
    db 11001111b        ; Flags: Granularity, 32-bit
    db 0x00             ; Base High

    ; Code Segment Descriptor (Long Mode, base 0)
    dw 0xFFFF           ; Limit Low
    dw 0x0000           ; Base Low
    db 0x00             ; Base Middle
    db 10011010b        ; Access Byte: Code Segment, Executable, Readable
    db 00100000b        ; Flags: Long mode
    db 0x00             ; Base High

    ; Data Segment Descriptor (Long Mode, base 0)
    dw 0xFFFF           ; Limit Low
    dw 0x0000           ; Base Low
    db 0x00             ; Base Middle
    db 10010010b        ; Access Byte: Data Segment, Writable
    db 00000000b        ; Flags
    db 0x00             ; Base High

gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1  ; Limit
    dd gdt_start                ; Base

; ----------------------------
; 32-bit Protected Mode Code
; ----------------------------
[BITS 32]
protected_mode:
    ; Update Segment Registers
    mov ax, 0x10        ; Data Segment Selector (2nd descriptor)
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Initialize Stack
    mov esp, 0x9FC00    ; Set stack pointer (adjust as needed)

    ; Display message in protected mode
    mov esi, protected_mode_msg
    call print_string_pm

    ; Set Up Paging Structures
    ; We'll identity map the first 4 GB for simplicity
    ; Allocate page tables (must be 4 KB aligned)
    ; For simplicity, we'll define them statically

    ; Page Alignment
    align 4096

pml4_table:
    dq pml3_table + 0x03    ; Present, Read/Write

pml3_table:
    times 512 dq 0          ; Zero out entries

    ; Map the first 512 GB (sufficient for our needs)
    mov dword [pml3_table], pml2_table + 0x03  ; Present, Read/Write

pml2_table:
    times 512 dq 0          ; Zero out entries

    ; Map the first 1 GB using 2 MB pages
    ; Set up entries in PML2 table
    ; Each entry covers 2 MB

    ; For simplicity, we'll identity map the first 1 GB
    mov ecx, 512            ; Number of entries
    mov esi, 0              ; Starting address
    mov edi, pml2_table     ; PML2 table address
set_pml2_entries:
    mov eax, esi
    or eax, 0x83            ; Present, Read/Write, Page Size
    mov [edi], eax
    add esi, 0x200000       ; Next 2 MB
    add edi, 8              ; Next entry
    loop set_pml2_entries

    ; Enable Long Mode
    ; Set LME bit in EFER
    mov ecx, 0xC0000080     ; MSR for EFER
    rdmsr
    or eax, 0x00000100      ; Set LME bit (bit 8)
    wrmsr

    ; Load PML4 Table into CR3
    mov eax, pml4_table
    mov cr3, eax

    ; Enable Paging
    mov eax, cr4
    or eax, 0x00000020      ; Set PAE bit in CR4
    mov cr4, eax

    mov eax, cr0
    or eax, 0x80000001      ; Set PG and PE bits
    mov cr0, eax

    ; Far Jump to Long Mode Code
    jmp 0x28:long_mode_start    ; Selector for long mode code segment

; ----------------------------
; 64-bit Long Mode Code
; ----------------------------
[BITS 64]
long_mode_start:
    ; Update Segment Registers
    mov ax, 0x30        ; Data Segment Selector (4th descriptor)
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Initialize Stack
    mov rsp, 0x9FC00    ; Set stack pointer (adjust as needed)

    ; Display message in long mode
    mov rsi, long_mode_msg
    call print_string_lm

    ; Infinite Loop
hang:
    jmp hang

; ----------------------------
; Helper Functions
; ----------------------------
[BITS 16]
print_string:
    mov ah, 0x0E        ; BIOS teletype output
.print_char:
    lodsb
    cmp al, 0
    je .done
    int 0x10
    jmp .print_char
.done:
    ret

[BITS 32]
print_string_pm:
    mov edx, 0xB8000
    mov bh, 0x07        ; Attribute byte
.next_char_pm:
    lodsb
    cmp al, 0
    je .done_pm
    mov [edx], al
    inc edx
    mov [edx], bh
    inc edx
    jmp .next_char_pm
.done_pm:
    ret

[BITS 64]
print_string_lm:
    mov rdx, 0xB8000
    mov bh, 0x07        ; Attribute byte
.next_char_lm:
    lodsb
    cmp al, 0
    je .done_lm
    mov [rdx], al
    inc rdx
    mov [rdx], bh
    inc rdx
    jmp .next_char_lm
.done_lm:
    ret

; ----------------------------
; Messages
; ----------------------------
[BITS 16]
real_mode_msg db "Real Mode", 0

[BITS 32]
protected_mode_msg db "Protected Mode", 0

[BITS 64]
long_mode_msg db "Long Mode", 0

; ----------------------------
; Bootloader Padding
; ----------------------------
[BITS 16]
times 510 - ($ - $$) db 0
dw 0xAA55