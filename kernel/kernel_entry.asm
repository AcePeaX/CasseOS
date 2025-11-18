global _start;
[bits 32]
_start:

%include "./kernel-long-mode-transition.asm"

[bits 64]
start_kernel:
    [extern kernel_main] ; Define calling point. Must have same name as kernel.c 'main' function
    call kernel_main ; Calls the C function. The linker will know where it is placed in memory
    jmp $

%define KERNEL_BOOTINFO_SIZE 56

global kernel_uefi_entry
kernel_uefi_entry:
    cli
    extern kernel_bootinfo
    extern kernel_main
    mov rsi, rdi                ; source pointer from loader
    lea rdi, [rel kernel_bootinfo]
    mov rcx, KERNEL_BOOTINFO_SIZE / 8
    rep movsq
    call kernel_main
.hang:
    hlt
    jmp .hang
