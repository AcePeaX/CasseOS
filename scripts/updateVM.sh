#!/bin/bash

# Define paths
BOOTLOADER="boot.asm"
OUTPUT_BIN="boot.bin"
DISK_IMAGE="boot.vdi"

# Assemble the bootloader
nasm -f bin $BOOTLOADER -o $OUTPUT_BIN

# Update the virtual disk image
# Convert boot.bin into a VDI and replace the existing boot.vdi
VBoxManage convertfromraw $OUTPUT_BIN $DISK_IMAGE --format VDI --existing

# Restart the VirtualBox VM
VBoxManage controlvm $VM_NAME poweroff
VBoxManage startvm $VM_NAME --type headless  # Use headless mode to run without a GUI
