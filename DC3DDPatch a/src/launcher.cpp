/*
 * DC3DD Patch Launcher
 *
 * - Windows: Uses embedded Locale Emulator (JP locale) then injects patch DLL.
 * - Wine/Proton: Uses registry + LC_ALL/LANG fallback then injects patch DLL.
 */

#define WIN32_LEAN_AND_MEAN
#include "resource.h"
#include <stdio.h>
#include <string.h>
#include <windows.h>

// ============================================================================
// Wine/Proton detection
// ============================================================================
typedef const char *(CDECL *pfnWineGetVersion)(void);

static bool IsRunningUnderWine() {
  HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
  if (!hNtdll)
    return false;
  pfnWineGetVersion pWineGetVersion =
      (pfnWineGetVersion)GetProcAddress(hNtdll, "wine_get_version");
  return (pWineGetVersion != NULL);
}

// ============================================================================
// Locale Emulator structures
// ============================================================================
#pragma pack(push, 1)

struct TIME_FIELDS {
  USHORT Year;
  USHORT Month;
  USHORT Day;
  USHORT Hour;
  USHORT Minute;
  USHORT Second;
  USHORT Milliseconds;
  USHORT Weekday;
};

struct RTL_TIME_ZONE_INFORMATION {
  LONG Bias;
  WCHAR StandardName[32];
  TIME_FIELDS StandardDate;
  LONG StandardBias;
  WCHAR DaylightName[32];
  TIME_FIELDS DaylightDate;
  LONG DaylightBias;
};

struct LEB {
  DWORD AnsiCodePage;
  DWORD OemCodePage;
  DWORD LocaleID;
  DWORD DefaultCharset;
  DWORD HookUILanguageAPI;
  BYTE DefaultFaceName[64];
  RTL_TIME_ZONE_INFORMATION Timezone;
};

#pragma pack(pop)

typedef DWORD(WINAPI *pfnLeCreateProcess)(
    void *leb, LPCWSTR applicationName, LPCWSTR commandLine,
    LPCWSTR currentDirectory, DWORD creationFlags, LPSTARTUPINFOW startupInfo,
    LPPROCESS_INFORMATION processInformation, LPVOID processAttributes,
    LPVOID threadAttributes, LPVOID environment, LPVOID token);

// ============================================================================
// Extract embedded resource to file
// ============================================================================
static bool ExtractResource(HMODULE hModule, LPCSTR resourceName,
                            const char *outputPath) {
  HRSRC hRes = FindResourceA(hModule, resourceName, RT_RCDATA);
  if (!hRes)
    return false;
  HGLOBAL hData = LoadResource(hModule, hRes);
  if (!hData)
    return false;
  DWORD size = SizeofResource(hModule, hRes);
  void *data = LockResource(hData);
  if (!data || size == 0)
    return false;

  HANDLE hFile = CreateFileA(outputPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                             FILE_ATTRIBUTE_NORMAL, NULL);
  if (hFile == INVALID_HANDLE_VALUE)
    return false;

  DWORD written = 0;
  BOOL ok = WriteFile(hFile, data, size, &written, NULL);
  CloseHandle(hFile);
  return ok && (written == size);
}

// ============================================================================
// Inject DLL into a running process
// ============================================================================
static bool InjectDLL(HANDLE hProcess, const char *dllPath) {
  SIZE_T pathLen = strlen(dllPath) + 1;
  LPVOID remotePath = VirtualAllocEx(hProcess, NULL, pathLen,
                                     MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
  if (!remotePath) {
    MessageBoxA(NULL, "Failed to allocate memory in target process",
                "Injection Error", MB_ICONERROR);
    return false;
  }
  if (!WriteProcessMemory(hProcess, remotePath, dllPath, pathLen, NULL)) {
    MessageBoxA(NULL, "Failed to write to target process memory",
                "Injection Error", MB_ICONERROR);
    VirtualFreeEx(hProcess, remotePath, 0, MEM_RELEASE);
    return false;
  }
  HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
  LPVOID pLoadLibraryA = (LPVOID)GetProcAddress(hKernel32, "LoadLibraryA");
  if (!pLoadLibraryA) {
    MessageBoxA(NULL, "Failed to get LoadLibraryA address", "Injection Error",
                MB_ICONERROR);
    VirtualFreeEx(hProcess, remotePath, 0, MEM_RELEASE);
    return false;
  }
  HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0,
                                      (LPTHREAD_START_ROUTINE)pLoadLibraryA,
                                      remotePath, 0, NULL);
  if (!hThread) {
    char msg[256];
    sprintf_s(msg, "Failed to create remote thread. Error: %lu",
              GetLastError());
    MessageBoxA(NULL, msg, "Injection Error", MB_ICONERROR);
    VirtualFreeEx(hProcess, remotePath, 0, MEM_RELEASE);
    return false;
  }
  WaitForSingleObject(hThread, 5000);
  CloseHandle(hThread);
  VirtualFreeEx(hProcess, remotePath, 0, MEM_RELEASE);
  return true;
}

// ============================================================================
// Set Japanese locale in registry (Wine/Proton path)
// ============================================================================
static void SetJapaneseLocaleRegistry() {
  HKEY hKey;
  if (RegCreateKeyExA(HKEY_CURRENT_USER, "Control Panel\\International", 0,
                      NULL, 0, KEY_SET_VALUE, NULL, &hKey,
                      NULL) == ERROR_SUCCESS) {
    RegSetValueExA(hKey, "ACP", 0, REG_SZ, (const BYTE *)"932", 4);
    RegSetValueExA(hKey, "OEMCP", 0, REG_SZ, (const BYTE *)"932", 4);
    RegSetValueExA(hKey, "MACCP", 0, REG_SZ, (const BYTE *)"10001", 6);
    RegSetValueExA(hKey, "Locale", 0, REG_SZ, (const BYTE *)"00000411", 9);
    RegSetValueExA(hKey, "LocaleName", 0, REG_SZ, (const BYTE *)"ja-JP", 6);
    RegCloseKey(hKey);
  }
  if (RegCreateKeyExA(HKEY_CURRENT_USER, "Control Panel\\Nls\\CodePage", 0,
                      NULL, 0, KEY_SET_VALUE, NULL, &hKey,
                      NULL) == ERROR_SUCCESS) {
    RegSetValueExA(hKey, "ACP", 0, REG_SZ, (const BYTE *)"932", 4);
    RegSetValueExA(hKey, "OEMCP", 0, REG_SZ, (const BYTE *)"932", 4);
    RegCloseKey(hKey);
  }
}

// ============================================================================
// Create process with Japanese locale via environment (Wine/Proton path)
// ============================================================================
static bool CreateProcessWithLC_ALL(const char *exePath, const char *currentDir,
                                    PROCESS_INFORMATION *pi) {
  SetJapaneseLocaleRegistry();

  LPCH envBlock = GetEnvironmentStringsA();
  if (!envBlock)
    return false;

  SIZE_T envSize = 0;
  {
    const char *p = envBlock;
    while (*p) {
      SIZE_T len = strlen(p) + 1;
      envSize += len;
      p += len;
    }
    envSize++;
  }

  const char *lcAll = "LC_ALL=ja_JP.UTF-8";
  const char *lang = "LANG=ja_JP.UTF-8";
  SIZE_T lcAllLen = strlen(lcAll) + 1;
  SIZE_T langLen = strlen(lang) + 1;

  char *newEnv = (char *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
                                   envSize + lcAllLen + langLen + 64);
  if (!newEnv) {
    FreeEnvironmentStringsA(envBlock);
    return false;
  }

  char *dst = newEnv;
  const char *src = envBlock;
  while (*src) {
    SIZE_T varLen = strlen(src) + 1;
    if (_strnicmp(src, "LC_ALL=", 7) != 0 && _strnicmp(src, "LANG=", 5) != 0) {
      memcpy(dst, src, varLen);
      dst += varLen;
    }
    src += varLen;
  }
  memcpy(dst, lcAll, lcAllLen);
  dst += lcAllLen;
  memcpy(dst, lang, langLen);
  dst += langLen;
  *dst = '\0';

  FreeEnvironmentStringsA(envBlock);

  STARTUPINFOA siA = {sizeof(siA)};
  *pi = {};
  BOOL ok = CreateProcessA(exePath, NULL, NULL, NULL, FALSE, CREATE_SUSPENDED,
                           newEnv, currentDir, &siA, pi);

  HeapFree(GetProcessHeap(), 0, newEnv);
  return ok ? true : false;
}

// ============================================================================
// Launch with embedded Locale Emulator (Windows path)
// ============================================================================
static bool LaunchWithLocaleEmulator(HINSTANCE hInstance, const char *exePath,
                                     const char *currentDir,
                                     PROCESS_INFORMATION *pi) {
  char loaderDllPath[MAX_PATH];
  char localeEmuPath[MAX_PATH];
  char tempDir[MAX_PATH];
  GetTempPathA(MAX_PATH, tempDir);
  sprintf_s(loaderDllPath, "%sLoaderDll.dll", tempDir);
  sprintf_s(localeEmuPath, "%sLocaleEmulator.dll", tempDir);

  if (GetFileAttributesA(loaderDllPath) == INVALID_FILE_ATTRIBUTES) {
    if (!ExtractResource(hInstance, MAKEINTRESOURCEA(IDR_LOADERDLL),
                         loaderDllPath)) {
      return false;
    }
  }

  if (GetFileAttributesA(localeEmuPath) == INVALID_FILE_ATTRIBUTES) {
    if (!ExtractResource(hInstance, MAKEINTRESOURCEA(IDR_LOCALEEMU),
                         localeEmuPath)) {
      return false;
    }
  }

  HMODULE hLoaderDll = LoadLibraryA(loaderDllPath);
  if (!hLoaderDll) {
    return false;
  }

  pfnLeCreateProcess pLeCreateProcess =
      (pfnLeCreateProcess)GetProcAddress(hLoaderDll, "LeCreateProcess");
  if (!pLeCreateProcess) {
    FreeLibrary(hLoaderDll);
    return false;
  }

  LEB leb = {};
  leb.AnsiCodePage = 932;
  leb.OemCodePage = 932;
  leb.LocaleID = 0x0411;
  leb.DefaultCharset = 128; // SHIFTJIS_CHARSET
  leb.HookUILanguageAPI = 0;
  memset(&leb.Timezone, 0, sizeof(leb.Timezone));
  leb.Timezone.Bias = -540;
  wcscpy_s(leb.Timezone.StandardName, 32, L"Tokyo Standard Time");
  wcscpy_s(leb.Timezone.DaylightName, 32, L"Tokyo Standard Time");

  wchar_t wExePath[MAX_PATH], wCurrentDir[MAX_PATH], wCommandLine[MAX_PATH * 2];
  MultiByteToWideChar(CP_ACP, 0, exePath, -1, wExePath, MAX_PATH);
  MultiByteToWideChar(CP_ACP, 0, currentDir, -1, wCurrentDir, MAX_PATH);
  swprintf_s(wCommandLine, L"\"%s\"", wExePath);

  STARTUPINFOW si = {sizeof(si)};
  *pi = {};
  BYTE lebBuffer[sizeof(LEB) + 4] = {};
  memcpy(lebBuffer, &leb, sizeof(LEB));

  DWORD ret = pLeCreateProcess(lebBuffer, wExePath, wCommandLine, wCurrentDir,
                               0x00000004, &si, pi, NULL, NULL, NULL, NULL);

  FreeLibrary(hLoaderDll);
  return (ret == 0);
}

// ============================================================================
// Plain fallback launcher (Windows)
// ============================================================================
static bool CreateProcessSuspendedPlain(const char *exePath,
                                        const char *currentDir,
                                        PROCESS_INFORMATION *pi) {
  STARTUPINFOA siA = {sizeof(siA)};
  *pi = {};
  BOOL ok = CreateProcessA(exePath, NULL, NULL, NULL, FALSE, CREATE_SUSPENDED,
                           NULL, currentDir, &siA, pi);
  return ok ? true : false;
}

// ============================================================================
// Main
// ============================================================================
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine,
                   int nCmdShow) {
  (void)hPrevInstance;
  (void)lpCmdLine;
  (void)nCmdShow;

  char currentDir[MAX_PATH];
  GetCurrentDirectoryA(MAX_PATH, currentDir);

  char exePath[MAX_PATH];
  char dllPath[MAX_PATH];
#ifdef GAME_DC4PH
  sprintf_s(exePath, "%s\\DC4PHDL.EXE", currentDir);
  sprintf_s(dllPath, "%s\\DC4PHPatch.dll", currentDir);

  if (GetFileAttributesA(exePath) == INVALID_FILE_ATTRIBUTES) {
    MessageBoxA(NULL,
                "DC4PHDL.EXE not found!\nPlease place this launcher in the DC4 "
                "Plus Harmony game folder.",
                "DC4PH Patch Launcher", MB_ICONERROR);
    return 1;
  }
  if (GetFileAttributesA(dllPath) == INVALID_FILE_ATTRIBUTES) {
    MessageBoxA(NULL,
                "DC4PHPatch.dll not found!\nPlease place DC4PHPatch.dll in the "
                "game folder.",
                "DC4PH Patch Launcher", MB_ICONERROR);
    return 1;
  }
#else
  sprintf_s(exePath, "%s\\DC3DD.EXE", currentDir);
  sprintf_s(dllPath, "%s\\DC3DDPatch.dll", currentDir);

  if (GetFileAttributesA(exePath) == INVALID_FILE_ATTRIBUTES) {
    MessageBoxA(
        NULL,
        "DC3DD.EXE not found!\nPlease place this launcher in the D.C.III Dream "
        "Days game folder.",
        "DC3DD Patch Launcher", MB_ICONERROR);
    return 1;
  }
  if (GetFileAttributesA(dllPath) == INVALID_FILE_ATTRIBUTES) {
    MessageBoxA(
        NULL,
        "DC3DDPatch.dll not found!\nPlease place DC3DDPatch.dll in the game "
        "folder.",
        "DC3DD Patch Launcher", MB_ICONERROR);
    return 1;
  }
#endif

  PROCESS_INFORMATION pi = {};
  bool isWine = IsRunningUnderWine();

  if (isWine) {
    if (!CreateProcessWithLC_ALL(exePath, currentDir, &pi)) {
      char msg[256];
      sprintf_s(msg, "Failed to start game under Wine. Error: %lu",
                GetLastError());
      MessageBoxA(NULL, msg, "Patch Launcher", MB_ICONERROR);
      return 1;
    }
  } else {
    // Windows: use embedded LE first, then plain fallback.
    if (!LaunchWithLocaleEmulator(hInstance, exePath, currentDir, &pi)) {
      if (!CreateProcessSuspendedPlain(exePath, currentDir, &pi)) {
        char msg[256];
        sprintf_s(msg, "Failed to start game. Error: %lu", GetLastError());
        MessageBoxA(NULL, msg, "Patch Launcher", MB_ICONERROR);
        return 1;
      }
    }
  }

  bool injected = InjectDLL(pi.hProcess, dllPath);
  ResumeThread(pi.hThread);
  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);

  if (!injected) {
    MessageBoxA(NULL,
                "DLL injection failed, but game is running without the patch.",
                "Patch Launcher", MB_ICONWARNING);
    return 1;
  }
  return 0;
}
