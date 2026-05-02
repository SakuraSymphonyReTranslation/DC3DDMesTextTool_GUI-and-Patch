@echo off
echo ==========================================
echo Building DC3DD Patch (No Locale Emulator)
echo ==========================================

if exist build_dc3dd rmdir /s /q build_dc3dd
mkdir build_dc3dd
cd build_dc3dd

echo [1/2] Configuring CMake...
cmake .. -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release
if %errorlevel% neq 0 (
    echo [ERROR] CMake failed.
    pause
    exit /b %errorlevel%
)

echo [2/2] Compiling...
nmake
if %errorlevel% neq 0 (
    echo [ERROR] Compilation failed.
    pause
    exit /b %errorlevel%
)

echo.
echo ==========================================
echo [SUCCESS] Build completed!
echo ==========================================
echo.
echo Output files:
echo   - build_dc3dd\DC3DDLauncher.exe
echo   - build_dc3dd\DC3DDPatch.dll
echo.
echo Installation:
echo   1. Copy BOTH files to the DC3DD game folder
echo   2. Run DC3DDLauncher.exe instead of DC3DD.EXE
echo.
pause
