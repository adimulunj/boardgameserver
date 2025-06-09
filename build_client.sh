#!/bin/bash

CLANG_COMPILER=clang++
SOURCE_FILE=client.cpp
OUTPUT_EXE=client.exe

echo "Compiling $SOURCE_FILE..."

$CLANG_COMPILER $SOURCE_FILE -o $OUTPUT_EXE -std=c++17

if [ $? -ne 0 ]; then
    echo "Compilation failed."
else
    echo "Compilation succeeded! Run with: ./$OUTPUT_EXE"
fi
