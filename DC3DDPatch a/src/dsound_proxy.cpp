/*
 * DC3DD Patch - dsound.dll Proxy
 *
 * Forwards all dsound exports to the renamed system DLL (dsound_original.dll).
 * This ensures the patch DLL is loaded by the OS loader BEFORE WinMain runs,
 * guaranteeing GetGlyphOutlineA hooks are active when the engine builds its font atlas.
 *
 * Setup:
 *   1. Copy C:\Windows\SysWOW64\dsound.dll -> game folder\dsound_original.dll
 *   2. Build this DLL as dsound.dll in the game folder
 *   Both done automatically by build_dc3dd.bat
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// Forward all dsound exports to dsound_original.dll (renamed system dsound)
#pragma comment(linker, "/export:DirectSoundCreate=dsound_original.DirectSoundCreate")
#pragma comment(linker, "/export:DirectSoundEnumerateA=dsound_original.DirectSoundEnumerateA")
#pragma comment(linker, "/export:DirectSoundEnumerateW=dsound_original.DirectSoundEnumerateW")
#pragma comment(linker, "/export:DllCanUnloadNow=dsound_original.DllCanUnloadNow")
#pragma comment(linker, "/export:DllGetClassObject=dsound_original.DllGetClassObject")
#pragma comment(linker, "/export:DirectSoundCaptureCreate=dsound_original.DirectSoundCaptureCreate")
#pragma comment(linker, "/export:DirectSoundCaptureEnumerateA=dsound_original.DirectSoundCaptureEnumerateA")
#pragma comment(linker, "/export:DirectSoundCaptureEnumerateW=dsound_original.DirectSoundCaptureEnumerateW")
#pragma comment(linker, "/export:DirectSoundCreate8=dsound_original.DirectSoundCreate8")
#pragma comment(linker, "/export:DirectSoundCaptureCreate8=dsound_original.DirectSoundCaptureCreate8")
#pragma comment(linker, "/export:DirectSoundFullDuplexCreate=dsound_original.DirectSoundFullDuplexCreate")
#pragma comment(linker, "/export:GetDeviceID=dsound_original.GetDeviceID")
