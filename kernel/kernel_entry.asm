[bits 32]
[extern main] ; Define calling point. Must have same name as kernel.c 'main' function
mov eax, 10
call main ; Calls the C function. The linker will know where it is placed in memory
jmp $

