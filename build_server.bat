@echo off
REM Batch script to compile multi-client server with Clang on Windows

REM Set paths (optional if Clang is already in PATH)
set CLANG_COMPILER=clang++
set SOURCE_FILE=server.cpp
set OUTPUT_EXE=server.exe

echo Compiling %SOURCE_FILE%...

%CLANG_COMPILER% %SOURCE_FILE% -o %OUTPUT_EXE% -std=c++17 -lws2_32

if %ERRORLEVEL% NEQ 0 (
    echo Compilation failed.
) else (
    echo Compilation succeeded! Running: %OUTPUT_EXE%
    pause
    server.exe
)


