#!/bin/sh
# Test script for assignment 3 inside QEMU
# Reads configuration from conf/assignment.txt and conf/username.txt

set -e
set -u

NUMFILES=10
WRITESTR="AELD_IS_FUN"
WRITEDIR=/tmp/aeld-data

# Load configuration
if [ -f "conf/assignment.txt" ]; then
    . ./conf/assignment.txt
fi

if [ -f "conf/username.txt" ]; then
    username=$(cat conf/username.txt)
else
    username="student"
fi

# Override with command-line arguments
if [ $# -ge 1 ]; then
    NUMFILES=$1
fi
if [ $# -ge 2 ]; then
    WRITESTR=$2
fi
if [ $# -ge 3 ]; then
    WRITEDIR=/tmp/aeld-data/$3
fi

MATCHSTR="The number of files are ${NUMFILES} and the number of matching lines are ${NUMFILES}"

echo "Writing ${NUMFILES} files containing string ${WRITESTR} to ${WRITEDIR}"

rm -rf "${WRITEDIR}"

# Create WRITEDIR if assignment is not assignment1
if [ "$assignment" != 'assignment1' ]; then
    mkdir -p "$WRITEDIR"
    if [ -d "$WRITEDIR" ]; then
        echo "$WRITEDIR created"
    else
        exit 1
    fi
fi

for i in $(seq 1 $NUMFILES); do
    ./writer "${WRITEDIR}/${username}${i}.txt" "${WRITESTR}"
done

OUTPUTSTRING=$(./finder.sh "${WRITEDIR}" "${WRITESTR}")

# Clean up
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
