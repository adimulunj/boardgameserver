@echo off
set CLANG_COMPILER=clang++
set SOURCE_FILE=client.cpp
set OUTPUT_EXE=client.exe

echo Compiling %SOURCE_FILE%...

%CLANG_COMPILER% %SOURCE_FILE% -o %OUTPUT_EXE% -std=c++17 -lws2_32 -lUser32

if %ERRORLEVEL% NEQ 0 (
    echo  Compilation failed.
) else (
    echo Compilation succeeded! Run with: %OUTPUT_EXE% "
)