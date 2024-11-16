global _start;
[bits 32]
_start:

paging_set_up:

%ifndef PML4T_ADDR
%define PML4T_ADDR 0x1000
%endif
    mov edi, PML4T_ADDR     ; PML4T page address
    mov cr3, edi
    xor eax, eax
    rep stosd               ; Now actually zero out the page table entries
    ; Set edi back to PML4T[0]
    mov edi, cr3

    mov dword[edi], 0x2003      ; Set PML4T[0] to address 0x2000 (PDPT) with flags 0x0003
    add edi, 0x1000             ; Go to PDPT[0]
    mov dword[edi], 0x3003      ; Set PDPT[0] to address 0x3000 (PDT) with flags 0x0003
    add edi, 0x1000             ; Go to PDT[0]
    mov dword[edi], 0x4003      ; Set PDT[0] to address 0x4000 (PT) with flags 0x0003

    mov edi, 0x4000             ; Go to PT[0]
    mov ebx, 0x00000003         ; EBX has address 0x0000 with flags 0x0003
    mov ecx, 512                ; Do the operation 512 times

    add_page_entry_protected:
        ; a = address, x = index of page table, flags are entry flags
        mov dword[edi], ebx                 ; Write ebx to PT[x] = a.append(flags)
        add ebx, 0x1000                     ; Increment address of ebx (a+1)
        add edi, 8                          ; Increment page table location (since entries are 8 bytes)
                                            ; x++
        loop add_page_entry_protected       ; Decrement ecx and loop again


    ; Set up PAE paging, but don't enable it quite yet
    ;
    ; Here we're basically telling the CPU that we want to use paging, but not quite yet.
    ; We're enabling the feature, but not using it.
    mov eax, cr4
    or eax, 1 << 5               ; Set the PAE-bit, which is the 5th bit
    mov cr4, eax


    [extern kernel_main] ; Define calling point. Must have same name as kernel.c 'main' function
    call kernel_main ; Calls the C function. The linker will know where it is placed in memory
    jmp $

