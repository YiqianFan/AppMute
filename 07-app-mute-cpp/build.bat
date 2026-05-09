@echo off
mkdir build 2>nul
cd build
cmake .. -G "Visual Studio 17 2022"
cmake --build . --config Release
if %ERRORLEVEL% EQU 0 (
    echo Build successful.
) else (
    echo Build failed.
)
pause