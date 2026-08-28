@echo off
echo Compiling Phase 1...
g++ src/main.cpp src/DataManager.cpp -o Phase1.exe
if %ERRORLEVEL% EQU 0 (
    echo Compilation successful! Running Phase1.exe...
    echo.
    .\Phase1.exe
) else (
    echo Compilation failed!
)
