%ifndef PML4T_ADDR
%define PML4T_ADDR 0x1000
%endif

%define PDPT_ADDR 0x2000
%define PDT_ADDR  0x3000

global kernel_setup_uefi_environment

extern gdt_64_descriptor

kernel_setup_uefi_environment:
    ; zero page table pages
    mov rdi, PML4T_ADDR
    mov rcx, (3 * 0x1000) / 8
    xor rax, rax
    rep stosq

    mov rax, PML4T_ADDR
    mov qword [rax], (PDPT_ADDR | 0x3)
    mov qword [PDPT_ADDR], (PDT_ADDR | 0x3)

    mov rdi, PDT_ADDR
    mov rcx, 512
    mov rbx, 0x0000000000000083        ; 2MB page, RW+P
.map_loop:
    mov qword [rdi], rbx
    add rbx, 0x200000
    add rdi, 8
    loop .map_loop

    mov rax, PML4T_ADDR
    mov cr3, rax

    mov rax, cr4
    or rax, 1 << 5
    mov cr4, rax

    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr

    mov rax, cr0
    or rax, 1 << 31
    mov cr0, rax

    lgdt [gdt_64_descriptor]
    mov ax, 0x10
    mov ds, ax
    mov ss, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    ret
