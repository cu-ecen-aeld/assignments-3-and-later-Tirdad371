#!/bin/bash

# Check if exactly two arguments are provided
if [ $# -ne 2 ]; then
    echo "Error: Two arguments required: <writefile> <writestr>"
    exit 1
fi

writefile=$1
writestr=$2

# Create directory path if needed
mkdir -p "$(dirname "$writefile")"
if [ $? -ne 0 ]; then
    echo "Error: Could not create directory for $writefile"
    exit 1
fi

# Write the string to the file (overwrite)
echo "$writestr" > "$writefile"
if [ $? -ne 0 ]; then
    echo "Error: Could not write to file $writefile"
    exit 1
fi

exit 0
