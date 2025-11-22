global _start;
[bits 32]
_start:

%include "./kernel-long-mode-transition.asm"

extern kernel_main
extern kernel_bootinfo

[bits 64]
start_kernel:
    and rsp, 0xFFFFFFFFFFFFFFF0
    call kernel_main           ; Calls the C function. The linker will know where it is placed in memory
    jmp $

%define KERNEL_BOOTINFO_SIZE 56

global kernel_uefi_entry
kernel_uefi_entry:
    cli
    lea rax, [rel gdt_64_descriptor]
    lgdt [rax]

    mov ax, data_seg_64
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov gs, ax

    push qword code_seg_64
    lea rax, [rel .reload_cs_done]
    push rax
    retfq
.reload_cs_done:

    mov rsi, rdi                ; source pointer from loader
    lea rdi, [rel kernel_bootinfo]
    mov rcx, KERNEL_BOOTINFO_SIZE / 8
    rep movsq
    call kernel_main
.hang:
    hlt
    jmp .hang
