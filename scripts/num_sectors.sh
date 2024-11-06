#!/bin/bash



FILE_SIZE=$(stat -c%s $KERNEL_BIN_PATH)
SECTOR_SIZE=512
NUM_SECTORS=$(( (FILE_SIZE + SECTOR_SIZE - 1) / SECTOR_SIZE ))
echo "Kernel size: $FILE_SIZE bytes"
echo "Number of sectors needed: $NUM_SECTORS"