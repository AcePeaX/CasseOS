    call detect_lm_protected
paging_set_up:

%ifndef PML4T_ADDR
%define PML4T_ADDR 0x1000
%endif
%define PDPT_ADDR (PML4T_ADDR + 0x1000)
%define PD_ADDR   (PDPT_ADDR + 0x1000)
%define NUM_PD_TABLES 4
%define print_protected 0x7ceb

    mov edi, PML4T_ADDR
    mov cr3, edi
    xor eax, eax
    mov ecx, (NUM_PD_TABLES + 2) * (4096 / 4)
    rep stosd

    mov edi, PML4T_ADDR
    mov eax, PDPT_ADDR
    or eax, 0x3
    mov dword [edi], eax
    mov dword [edi + 4], 0

    mov edi, PDPT_ADDR
    mov ecx, NUM_PD_TABLES
    mov ebx, PD_ADDR
.fill_pdpt:
    mov eax, ebx
    or eax, 0x3
    mov dword [edi], eax
    mov dword [edi + 4], 0
    add edi, 8
    add ebx, 0x1000
    loop .fill_pdpt

    mov edi, PD_ADDR
    xor eax, eax
    mov ecx, NUM_PD_TABLES * 512
.fill_pds:
    mov edx, eax
    or edx, 0x83                  ; present | rw | PS
    mov dword [edi], edx
    mov dword [edi + 4], 0
    add edi, 8
    add eax, 0x200000             ; advance 2MB
    loop .fill_pds


    ; Set up PAE paging, but don't enable it quite yet
    ;
    ; Here we're basically telling the CPU that we want to use paging, but not quite yet.
    ; We're enabling the feature, but not using it.
    mov eax, cr4
    or eax, 1 << 5               ; Set the PAE-bit, which is the 5th bit
    mov cr4, eax

elevate_to_lm:
    ; Elevate to 64-bit mode
    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr

    ; Enable paging
    mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax
    
    lgdt [gdt_64_descriptor]
    jmp code_seg_64:start_kernel


detect_lm_protected:
    pushad

    ; Check for CPUID
    ; Read from FLAGS
    pushfd                          ; Copy FLAGS to eax via stack
    pop eax

    ; Save to ecx for comparison later
    mov ecx, eax

    ; Flip the ID bit (21st bit of eax)
    xor eax, 1 << 21

    ; Write to FLAGS
    push eax
    popfd

    ; Read from FLAGS again
    ; Bit will be flipped if CPUID supported
    pushfd
    pop eax

    ; Restore eflags to the older version saved in ecx
    push ecx
    popfd

    ; Perform the comparison
    ; If equal, then the bit got flipped back during copy,
    ; and CPUID is not supported
    cmp eax, ecx
    je cpuid_not_found_protected        ; Print error and hang if CPUID unsupported


    ; Check for extended functions of CPUID
    mov eax, 0x80000000             ; CPUID argument than 0x80000000
    cpuid                           ; Run the command
    cmp eax, 0x80000001             ; See if result is larger than than 0x80000001
    jb cpuid_not_found_protected    ; If not, error and hang


    ; Actually check for long mode
    mov eax, 0x80000001             ; Set CPUID argument
    cpuid                           ; Run CPUID
    test edx, 1 << 29               ; See if bit 29 set in edx
    jz lm_not_found_protected       ; If it is not, then error and hang
    
    ; Return from the function
    popad
    ret


; Print an error message and hang
cpuid_not_found_protected:
    mov ebx, cpuid_not_found_str
    call print_protected
    jmp $


; Print an error message and hang
lm_not_found_protected:
    mov ebx, lm_not_found_str
    call print_protected
    jmp $

lm_not_found_str:                   db `ERROR: Long mode not supported. Exiting...             `, 0
cpuid_not_found_str:                db `ERROR: CPUID unsupported, but required for long mode     `, 0


align 8

global gdt_64_descriptor

gdt_64_start:

; Define the null sector for the 64 bit gdt
; Null sector is required for memory integrity check
gdt_64_null:
    dd 0x00000000           ; All values in null entry are 0
    dd 0x00000000           ; All values in null entry are 0

; Define the code sector for the 64 bit gdt
gdt_64_code:
    ; Base:     0x00000
    ; Limit:    0xFFFFF
    ; 1st Flags:        0b1001
    ;   Present:        1
    ;   Privelege:      00
    ;   Descriptor:     1
    ; Type Flags:       0b1010
    ;   Code:           1
    ;   Conforming:     0
    ;   Readable:       1
    ;   Accessed:       0
    ; 2nd Flags:        0b1100
    ;   Granularity:    1
    ;   32-bit Default: 0
    ;   64-bit Segment: 1
    ;   AVL:            0

    dw 0xFFFF           ; Limit (bits 0-15)
    dw 0x0000           ; Base  (bits 0-15)
    db 0x00             ; Base  (bits 16-23)
    db 0b10011010       ; 1st Flags, Type flags
    db 0b10101111       ; 2nd Flags, Limit (bits 16-19)
    db 0x00             ; Base  (bits 24-31)

; Define the data sector for the 64 bit gdt
gdt_64_data:
    ; Base:     0x00000
    ; Limit:    0x00000
    ; 1st Flags:        0b1001
    ;   Present:        1
    ;   Privelege:      00
    ;   Descriptor:     1
    ; Type Flags:       0b0010
    ;   Code:           0
    ;   Expand Down:    0
    ;   Writeable:      1
    ;   Accessed:       0
    ; 2nd Flags:        0b1100
    ;   Granularity:    1
    ;   32-bit Default: 0
    ;   64-bit Segment: 1
    ;   AVL:            0

    dw 0x0000           ; Limit (bits 0-15)
    dw 0x0000           ; Base  (bits 0-15)
    db 0x00             ; Base  (bits 16-23)
    db 0b10010010       ; 1st Flags, Type flags
    db 0b10100000       ; 2nd Flags, Limit (bits 16-19)
    db 0x00             ; Base  (bits 24-31)

gdt_64_end:

; Define the gdt descriptor
; This data structure gives cpu length and start address of gdt
; We will feed this structure to the CPU in order to set the protected mode GDT
gdt_64_descriptor:
    dw gdt_64_end - gdt_64_start - 1        ; Size of GDT, one byte less than true size
    dq gdt_64_start                         ; Start of the 64 bit gdt

; Define helpers to find pointers to Code and Data segments
code_seg_64:                            equ gdt_64_code - gdt_64_start
data_seg_64:                            equ gdt_64_data - gdt_64_start
