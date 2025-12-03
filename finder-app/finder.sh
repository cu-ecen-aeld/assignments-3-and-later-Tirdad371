#!/bin/sh

filesdir=$1
searchstr=$2

# Check that both arguments are provided
if [ -z "$filesdir" ] || [ -z "$searchstr" ]; then
    echo "Error: Missing arguments"
    echo "Usage: ./finder.sh <directory> <search_string>"
    exit 1
fi

# Check that filesdir is a directory
if [ ! -d "$filesdir" ]; then
    echo "Error: $filesdir is not a valid directory"
    exit 1
fi

# Count number of files
X=$(find "$filesdir" -type f | wc -l)

# Count number of matching lines
Y=$(grep -r "$searchstr" "$filesdir" 2>/dev/null | wc -l)

# Output result
echo "The number of files are $X and the number of matching lines are $Y"
