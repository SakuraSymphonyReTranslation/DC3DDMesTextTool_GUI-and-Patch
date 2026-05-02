@echo off
echo ==========================================
echo Building DC3DD Patch (Launcher + DLL)
echo   Output: build_dc3dd_win32\
echo ==========================================

if exist build_dc3dd_win32 rmdir /s /q build_dc3dd_win32
mkdir build_dc3dd_win32
cd build_dc3dd_win32

echo [1/4] Configuring CMake for DC3DD (Win32)...
cmake .. -G "Visual Studio 17 2022" -A Win32
if %errorlevel% neq 0 (
    echo [ERROR] CMake configuration failed.
    echo         Make sure Visual Studio 2022 is installed.
    cd ..
    pause
    exit /b %errorlevel%
)

echo [2/4] Compiling...
cmake --build . --config Release
if %errorlevel% neq 0 (
    echo [ERROR] Compilation failed.
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
if %errorlevel% neq 0 (
    echo [WARNING] One or more Locale Emulator files not found in parent folder.
    echo           Make sure LEProc.exe, LocaleEmulator.dll, LoaderDll.dll exist.
)

cd ..

echo.
echo ==========================================
echo [SUCCESS] Build completed!
echo ==========================================
echo.
echo Output files (build_dc3dd_win32\):
echo   - DC3DDLauncher.exe     (launcher with built-in LE)
echo   - DC3DDPatch.dll        (patch DLL)
echo   - LEProc.exe            (Locale Emulator process launcher)
echo   - LocaleEmulator.dll    (LE locale hook DLL)
echo   - LoaderDll.dll         (LE bootstrapper)
echo.
echo Installation:
echo   1. Copy ALL files from build_dc3dd_win32\ to the DC3DD game folder
echo   2. Run DC3DDLauncher.exe  (auto Japanese locale + patch)
echo   3. Save game data: AdvData\Savegame\
echo.
pause
