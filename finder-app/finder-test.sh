#!/bin/sh
# Tester script for assignment 1 and assignment 2
# Author: Tirdad

set -e
set -u

NUMFILES=10
WRITESTR=AELD_IS_FUN
WRITEDIR=/tmp/aeld-data
username=$(cat conf/username.txt)

if [ $# -lt 3 ]
then
    echo "Using default value ${WRITESTR} for string to write"
    if [ $# -lt 1 ]
    then
        echo "Using default value ${WRITEDIR} for files directory"
    else
        WRITEDIR=$1
    fi
else
    WRITESTR=$3
    WRITEDIR=$1
fi

MATCHSTR="The number of files are ${NUMFILES} and the number of matching lines are ${NUMFILES}"

# Remove and recreate the directory
rm -rf "$WRITEDIR"
mkdir -p "$WRITEDIR"

# Create files using the C writer application
i=1
while [ $i -le $NUMFILES ]
do
    ./writer "$WRITEDIR/$username$i.txt" "$WRITESTR"
    i=$((i + 1))
done

# Run finder and capture output
OUTPUTSTRING=$(./finder.sh "$WRITEDIR" "$WRITESTR")

# Remove temporary directories
rm -rf /tmp/aeld-data

set +e
echo "${OUTPUTSTRING}" | grep "${MATCHSTR}"
if [ $? -eq 0 ]; then
    echo "success"
    exit 0
else
    echo "failed: expected ${MATCHSTR} in ${OUTPUTSTRING} but instead found"
    exit 1
fi
