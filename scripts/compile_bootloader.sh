#!/bin/bash

# Define paths
BOOTLOADER="bootloader.asm"
OUTPUT_BIN="bootloader.bin"


# Assemble the bootloader
cd bootloader
nasm -f bin -g $BOOTLOADER -o ../bin/$OUTPUT_BIN
