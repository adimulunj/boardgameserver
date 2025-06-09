#!/bin/bash
# Shell script to compile multi-client server with Clang on Linux

CLANG_COMPILER=clang++
SOURCE_FILE=server.cpp
OUTPUT_EXE=server.exe

echo "Compiling $SOURCE_FILE..."

$CLANG_COMPILER $SOURCE_FILE -o $OUTPUT_EXE -std=c++17

if [ $? -ne 0 ]; then
    echo "Compilation failed."
else
    echo "Compilation succeeded! Running: ./$OUTPUT_EXE"
    ./$OUTPUT_EXE
fi
