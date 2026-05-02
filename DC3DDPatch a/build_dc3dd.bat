@echo off
setlocal enabledelayedexpansion

:: ==========================================
:: Build Script for DC3DD Patch (Launcher + DLL)
:: Uses MSVC (cl.exe) and CMake
:: ==========================================

echo ==========================================
echo Building DC3DD Patch (Launcher + DLL)
echo   Output: build_dc3dd\
echo ==========================================

:: Check if CMake is available
where cmake >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] CMake is not installed or not in PATH.
    pause
    exit /b 1
)

:: Create build directory
if not exist build_dc3dd mkdir build_dc3dd
cd build_dc3dd

echo [1/4] Configuring CMake for DC3DD (Win32)...
cmake -G "Visual Studio 17 2022" -A Win32 -DBUILD_DC4PH=OFF ..
if %errorlevel% neq 0 (
    echo [ERROR] CMake configuration failed.
    cd ..
    pause
    exit /b %errorlevel%
)

echo [2/4] Compiling Release build...
cmake --build . --config Release --target DC3DDPatch DC3DDLauncher
if %errorlevel% neq 0 (
    echo [ERROR] Build failed.
    cd ..
    pause
    exit /b %errorlevel%
)

echo [3/4] Collecting build artifacts...
copy /y ".\Release\DC3DDLauncher.exe" ".\DC3DDLauncher.exe" >nul
copy /y ".\Release\DC3DDPatch.dll"    ".\DC3DDPatch.dll"    >nul
if %errorlevel% neq 0 (
    echo [ERROR] Failed to copy compiled files.
    cd ..
    pause
    exit /b %errorlevel%
)

echo [4/4] Bundling Locale Emulator binaries...
copy /y "..\LEProc.exe"          ".\LEProc.exe"          >nul
copy /y "..\LocaleEmulator.dll"  ".\LocaleEmulator.dll"  >nul
copy /y "..\LoaderDll.dll"       ".\LoaderDll.dll"       >nul

echo.
echo ==========================================
echo [SUCCESS] Build completed!
echo ==========================================
echo.
echo Output files (build_dc3dd\):
echo   - DC3DDLauncher.exe     (launcher with built-in LE)
echo   - DC3DDPatch.dll        (patch DLL)
echo   - LEProc.exe            (Locale Emulator process launcher)
echo   - LocaleEmulator.dll    (LE locale hook DLL)
echo   - LoaderDll.dll         (LE bootstrapper)
echo.
echo Installation:
echo   1. Copy ALL files from build_dc3dd\ to the DC3DD game folder
echo   2. Run DC3DDLauncher.exe  (auto Japanese locale + patch)
echo   3. Save game data: AdvData\Savegame\
echo.

cd ..
pause
