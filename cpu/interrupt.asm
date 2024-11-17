; interrupt.asm
[BITS 64]
GLOBAL isr0
GLOBAL isr1
GLOBAL isr2
GLOBAL isr3
GLOBAL isr4
GLOBAL isr5
GLOBAL isr6
GLOBAL isr7
GLOBAL isr8
GLOBAL isr9
GLOBAL isr10
GLOBAL isr11
GLOBAL isr12
GLOBAL isr13
GLOBAL isr14
GLOBAL isr15
GLOBAL isr16
GLOBAL isr17
GLOBAL isr18
GLOBAL isr19
GLOBAL isr20
GLOBAL isr21
GLOBAL isr22
GLOBAL isr23
GLOBAL isr24
GLOBAL isr25
GLOBAL isr26
GLOBAL isr27
GLOBAL isr28
GLOBAL isr29
GLOBAL isr30
GLOBAL isr31

GLOBAL irq0
GLOBAL irq1
GLOBAL irq2
GLOBAL irq3
GLOBAL irq4
GLOBAL irq5
GLOBAL irq6
GLOBAL irq7
GLOBAL irq8
GLOBAL irq9
GLOBAL irq10
GLOBAL irq11
GLOBAL irq12
GLOBAL irq13
GLOBAL irq14
GLOBAL irq15
; ... up to irq15
EXTERN isr_common_stub_no_err
EXTERN isr_common_stub_err
EXTERN irq_common_stub
EXTERN isr_handler
EXTERN irq_handler

%macro PUSH_ALL 0
    push r15
    push r14
    push r13
    push r12
    push r11
    push r10
    push r9
    push r8
    push rbp
    push rdi
    push rsi
    push rdx
    push rcx
    push rbx
    push rax
%endmacro

%macro POP_ALL 0
    pop rax
    pop rbx
    pop rcx
    pop rdx
    pop rsi
    pop rdi
    pop rbp
    pop r8
    pop r9
    pop r10
    pop r11
    pop r12
    pop r13
    pop r14
    pop r15
%endmacro


%macro ISR_NO_ERR 1
isr%1:
    push qword 0          ; Dummy error code
    push qword %1         ; Interrupt number
    jmp isr_common_stub_no_err
%endmacro

%macro ISR_ERR 1
isr%1:
    push qword %1         ; Interrupt number
    jmp isr_common_stub_err
%endmacro

; ISRs without error codes
ISR_NO_ERR 0
ISR_NO_ERR 1
ISR_NO_ERR 2
ISR_NO_ERR 3
ISR_NO_ERR 4
ISR_NO_ERR 5
ISR_NO_ERR 6
ISR_NO_ERR 7
ISR_NO_ERR 9
ISR_NO_ERR 11
ISR_NO_ERR 15
ISR_NO_ERR 16
ISR_NO_ERR 18
ISR_NO_ERR 19
ISR_NO_ERR 20
ISR_NO_ERR 21
ISR_NO_ERR 22
ISR_NO_ERR 23
ISR_NO_ERR 24
ISR_NO_ERR 25
ISR_NO_ERR 26
ISR_NO_ERR 27
ISR_NO_ERR 28
ISR_NO_ERR 29
ISR_NO_ERR 30
ISR_NO_ERR 31

; ISRs with error codes
ISR_ERR 8
ISR_ERR 10
ISR_ERR 12
ISR_ERR 13
ISR_ERR 14
ISR_ERR 17

; IRQ stubs
%macro IRQ 1
irq%1:
    push qword 0          ; Dummy error code
    push qword (%1 + 32)  ; Interrupt number
    jmp irq_common_stub
%endmacro

; IRQs 0 to 15
IRQ 0
IRQ 1
IRQ 2
IRQ 3
IRQ 4
IRQ 5
IRQ 6
IRQ 7
IRQ 8
IRQ 9
IRQ 10
IRQ 11
IRQ 12
IRQ 13
IRQ 14
IRQ 15

; Common ISR stub for exceptions without error code
isr_common_stub_no_err:
    PUSH_ALL
    mov rdi, rsp            ; Pass pointer to registers_t
    sub rsp, 8              ; Align stack to 16 bytes
    call isr_handler
    add rsp, 8
    POP_ALL
    add rsp, 16             ; Remove error code and interrupt number
    iretq

; Common ISR stub for exceptions with error code
isr_common_stub_err:
    PUSH_ALL
    mov rdi, rsp
    sub rsp, 8
    call isr_handler
    add rsp, 8
    POP_ALL
    add rsp, 8              ; Remove interrupt number
    iretq

; Common IRQ stub
irq_common_stub:
    PUSH_ALL
    mov rdi, rsp
    sub rsp, 8
    call irq_handler
    add rsp, 8
    POP_ALL
    add rsp, 16             ; Remove error code and interrupt number
    iretq

