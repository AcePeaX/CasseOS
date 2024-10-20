#!/bin/bash

# Define paths
BOOTLOADER_FIRST="boot-first.asm"
BOOTLOADER_SECOND="boot-second.asm"
OUTPUT_BIN_FIRST="boot-first.bin"
OUTPUT_BIN_SECOND="boot-second.bin"
OUTPUT_BIN="boot.bin"
DISK_IMAGE="boot.vdi"
VM_NAME="CasseOS"  # Replace with your VirtualBox VM name
STORAGE_CONTROLLER="SATA"  # Replace with your controller name (e.g., SATA Controller)

# Assemble the bootloader
nasm -f bin $BOOTLOADER_FIRST -o ./bin/$OUTPUT_BIN_FIRST
nasm -f bin $BOOTLOADER_SECOND -o ./bin/$OUTPUT_BIN_SECOND

# Navigate to the bin directory
cd bin

# Concatenate the two bootloaders
cat $OUTPUT_BIN_FIRST $OUTPUT_BIN_SECOND > $OUTPUT_BIN

# Pad the bootloader and convert into a VDI format
dd if=/dev/zero of=boot.img bs=1M count=10
dd if=$OUTPUT_BIN of=boot.img conv=notrunc

# Detach the old disk from the VM
VBoxManage storageattach $VM_NAME \
    --storagectl "$STORAGE_CONTROLLER" \
    --port 0 --device 0 --medium none

# Remove existing medium from VirtualBox if it exists
VBoxManage closemedium disk $DISK_IMAGE --delete 2>/dev/null

# Convert the padded image into a VDI format
VBoxManage convertfromraw boot.img $DISK_IMAGE --format VDI


# Attach the new disk to the VM
VBoxManage storageattach $VM_NAME \
    --storagectl "$STORAGE_CONTROLLER" \
    --port 0 --device 0 --type hdd --medium $DISK_IMAGE

# Restart the VirtualBox VM
VBoxManage controlvm $VM_NAME poweroff 2>/dev/null
VBoxManage startvm $VM_NAME --type gui  # Use 'gui' to see the VM window
