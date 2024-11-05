#!/bin/bash

# Check if a file path is provided as an argument
if [ -z "$1" ]; then
    echo "Usage: $0 /path/to/file"
    exit 1
fi

# Get the directory path from the file path
file_path="$1"
dir_path=$(dirname "$file_path")

# Create the directory structure
mkdir -p "$dir_path"

