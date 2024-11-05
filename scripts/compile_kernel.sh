#!/bin/bash

# Define paths
KERNEL_C="kernel"
KERNEL_ENTRY="kernel_entry"
OUTPUT_KERNEL="kernel.bin"


# Compiling kernel.c
if [[ "$ELF_TYPE" == "i386" ]]; then
    # Execute commands for i386
    i386-elf-gcc -ffreestanding -c kernel/$KERNEL_C.c -o bin/$KERNEL_C.o
else
    # Execute commands for i686
    i686-elf-gcc -ffreestanding -c kernel/$KERNEL_C.c -o bin/$KERNEL_C.o
fi

nasm kernel/$KERNEL_ENTRY.asm -f elf -o bin/$KERNEL_ENTRY.o



# Linking
if [[ "$ELF_TYPE" == "i386" ]]; then
    # Execute commands for i386
    i386-elf-ld -o bin/$OUTPUT_KERNEL -Ttext 0x1000 bin/$KERNEL_ENTRY.o bin/$KERNEL_C.o --oformat binary
else
    # Execute commands for i686
    i686-elf-ld -o bin/$OUTPUT_KERNEL -Ttext 0x1000 bin/$KERNEL_ENTRY.o bin/$KERNEL_C.o --oformat binary
fi