#!/bin/bash

# Define paths
OUTPUT_BOOTLOADER_BIN="bootloader.bin"
OUTPUT_KERNEL_BIN="kernel.bin"
OUTPUT_BIN="os-image.bin"
DISK_IMAGE="os-image.vdi"
VM_NAME="CasseOS"  # Replace with your VirtualBox VM name
STORAGE_CONTROLLER="SATA"  # Replace with your controller name (e.g., SATA Controller)

# Default values
qemu=true
debug=false
virtualbox=false

# Parse command-line arguments
while [[ "$#" -gt 0 ]]; do
    case $1 in
        --qemu) qemu=true ;;  # If -a or --flag-a is passed, set flag_a to true
        --virtualbox) virtualbox=true; qemu=false ;;  # If -b or --flag-b is passed, set flag_b to true
        --debug) debug=true ;;  # If -b or --flag-b is passed, set flag_b to true
        *) echo "Unknown option: $1"; exit 1 ;;
    esac
    shift  # Shift to the next argument
done

if [ "$debug" = true ]; then
    if [ "$qemu" = false ]; then
        echo "Can only debug with qemu"; exit 1;
    fi
fi


# Navigate to the bin directory
cd bin

# Concatenate the bootloader and the kernel
cat $OUTPUT_BOOTLOADER_BIN $OUTPUT_KERNEL_BIN > $OUTPUT_BIN

# Pad the bootloader and convert into a VDI format
dd if=/dev/zero of=boot.img bs=1M count=10
dd if=$OUTPUT_BIN of=boot.img conv=notrunc
#dd if=$OUTPUT_BIN_SECOND of=boot.img bs=512 seek=2 conv=notrunc



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

if [ "$virtualbox" = true ]; then
    # Restart the VirtualBox VM
    VBoxManage controlvm $VM_NAME poweroff 2>/dev/null
    VBoxManage startvm $VM_NAME --type gui  # Use 'gui' to see the VM window
fi

if [ "$qemu" = true ]; then
    if [ "$debug" = true ]; then
        qemu-system-x86_64 -hda $DISK_IMAGE -monitor stdio -display sdl -S -s
    else
        qemu-system-x86_64 -hda $DISK_IMAGE -monitor stdio -display sdl
    fi
fi