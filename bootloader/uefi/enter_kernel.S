.text
.globl uefi_enter_kernel
.type uefi_enter_kernel, @function
uefi_enter_kernel:
    cli
    mov %rsi, %rsp
    mov %rsi, %rbp
    mov %rdi, %rax
    mov %rdx, %rdi
    jmp *%rax
