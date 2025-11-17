global _start;
[bits 32]
_start:

%include "./kernel-long-mode-transition.asm"

[bits 64]
start_kernel:
    [extern kernel_main] ; Define calling point. Must have same name as kernel.c 'main' function
    call kernel_main ; Calls the C function. The linker will know where it is placed in memory
    jmp $

global kernel_uefi_entry
kernel_uefi_entry:
    cli
    mov rsp, 0x80000
    mov rbp, rsp
    extern kernel_main
    call kernel_main
.hang:
    hlt
    jmp .hang



