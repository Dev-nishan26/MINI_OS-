@echo off
setlocal

if not exist build mkdir build

g++ -std=c++20 -Wall -Wextra -pedantic -Iinclude src\main.cpp src\Utils.cpp src\OS.cpp -o build\smaat.exe -lpsapi
if errorlevel 1 (
    echo Build failed.
    exit /b 1
)

echo Build finished: build\smaat.exe
