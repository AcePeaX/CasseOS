#!/bin/bash

verbose=true
while [[ "$#" -gt 0 ]]; do
    case $1 in
        --no-verbose) verbose=false ;;  # If -a or --flag-a is passed, set flag_a to true
        *) echo "Unknown option: $1"; exit 1 ;;
    esac
    shift  # Shift to the next argument
done

FILE_SIZE=$(stat -c%s $KERNEL_BIN_PATH)
SECTOR_SIZE=512
NUM_SECTORS=$(( (FILE_SIZE + SECTOR_SIZE - 1) / SECTOR_SIZE ))

if [ "$verbose" = true ]; then
    echo "Kernel size: $FILE_SIZE bytes"
    echo "Number of sectors needed: $NUM_SECTORS"
else
    echo "$NUM_SECTORS"
fi
