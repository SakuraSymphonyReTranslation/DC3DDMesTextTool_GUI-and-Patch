# DC4 Localization Notes (Indonesian Patch)

> Game: https://vndb.org/v20435
>
> | Tool | Description |
> |:---|:---|
> | [MesTextTool](https://github.com/cokkeijigen/MesTextTool) | Mes script text extraction/insertion |
> | [sagilio::GARbro](https://github.com/sagilio/GARbro) | *.CRX image conversion |
> | [x32dbg](https://x64dbg.com/) | Dynamic debugging & analysis |
> | IDA 9 PRO | Static reverse engineering |
> | CFF Explorer | IAT modification |
> | [Microsoft Detours](https://github.com/microsoft/Detours) | API hooking library |

> [!NOTE]
> This document follows the structure of [DC3WY.md](https://github.com/cokkeijigen/CircusEnginePatchs/blob/master/DC3WY.md) and adapts the techniques for DC4 (`D.C.4 ～ダ・カーポ4～`).
> DC4 uses the same Circus engine family but has different addresses and some structural differences.

---

## 0x00 Removing DVD and KEY Verification

### DVD Verification

The DVD verification can be found by searching for the string `DVD` in the executable, or by setting a breakpoint on `MessageBoxA`.

**Method (x32dbg):**

1. Open `DC4.EXE` in x32dbg
2. Go to **CPU tab** → Right-click → **Search for** → **All modules** → **String references**
3. Search/filter for `DVD`
4. You'll find a reference near a `MessageBoxA` call that shows the DVD check error dialog
5. Above the error dialog code, there will be a conditional jump (`je` / `jne` / `jz`) that branches to the error message when the DVD check fails
6. **NOP out the conditional jump** to skip the DVD check entirely

```
; Before (example):
je      <DVD_error_handler>      ; Jump to error if DVD check fails

; After:
nop                              ; NOP the jump (fill with 90)
nop
nop
nop
nop
nop
```

> [!TIP]
> In x32dbg, select the jump instruction → right-click → **Binary** → **Fill with NOPs**. This is the quickest way.

---

### KEY Verification (Serial Key Dialog)

The KEY verification popup can be found by searching for the string `InstKey` in x32dbg.

**Method:**

1. Set a breakpoint on `DialogBoxParamA` — this is the Win32 API that shows the serial key dialog
2. Run the game — it will break at the dialog call
3. Use **Alt+F9** (Run to user code) to return to the game's own code
4. You'll find a function (e.g., `sub_XXXXXX`) that calls `DialogBoxParamA`
5. This function is called in a loop — the game keeps showing the dialog until a valid key is entered
6. Find the `jmp` instruction that leads to the "key accepted" code path (usually after the loop)
7. Replace the `call <key_dialog_function>` with `jmp <key_accepted_path>` and NOP the remaining bytes

```
; Before:
call    <key_dialog_function>    ; Shows the serial key dialog in a loop
...
jmp     <key_accepted_path>      ; This is where valid key leads

; After:
jmp     <key_accepted_path>      ; Jump directly to accepted path
nop                              ; NOP remaining bytes
nop
nop
```

> [!IMPORTANT]
> Once you've patched the exe, save it: in x32dbg → **File** → **Patch file** → save as new exe.
> Always keep a backup of the original `DC4.EXE`.

---

## 0x01 System API Hooks (DLL Injection Approach)

Instead of modifying the exe directly (like DC3WY does for some patches), the DC4 patch uses **DLL injection via a Launcher** + **Microsoft Detours** for API hooking. This is more flexible and doesn't require modifying the game executable.

### Architecture

```
DC4Launcher.exe
  └── Starts DC4.EXE with DC4Patch.dll injected via CreateProcess + Detours
        └── DC4Patch.dll hooks Win32 APIs at runtime
```

### CreateFileA (File Redirection)

To achieve coexistence with original files, we hook `CreateFileA` to redirect file access to the `id_Data` folder (for Indonesian translations):

```cpp
static std::string ReplacePathA(const char* path) {
    if (!path) return {};
    std::string_view sv(path);
    if (sv.find("id_Data") != std::string_view::npos) return {};
    
    size_t pos = sv.find_last_of("\\/");
    if (pos != std::string_view::npos) {
        std::string newPath = ".\\id_Data";
        newPath += sv.substr(pos);
        if (GetFileAttributesA(newPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
            return newPath;  // File exists in id_Data, redirect!
        }
    }
    return {};  // Use original path
}

static HANDLE WINAPI Hook_CreateFileA(LPCSTR lpFileName, ...) {
    std::string newPath = ReplacePathA(lpFileName);
    if (!newPath.empty()) {
        return Real_CreateFileA(newPath.c_str(), ...);  // Redirected
    }
    return Real_CreateFileA(lpFileName, ...);  // Original
}
```

### GetGlyphOutlineA (Font Replacement)

The game uses Shift_JIS fonts. To display Indonesian (Latin) characters properly, we hook `GetGlyphOutlineA` and replace the game's font with a Unicode-capable font:

```cpp
static DWORD WINAPI Hook_GetGlyphOutlineA(HDC hdc, UINT uChar, UINT fuFormat, 
    LPGLYPHMETRICS lpgm, DWORD cjBuffer, LPVOID pvBuffer, const MAT2* lpmat2) {
    TEXTMETRICA tm;
    if (GetTextMetricsA(hdc, &tm)) {
        HFONT f = g_fontManager.GetFont(tm.tmHeight);
        if (f) {
            HFONT old = (HFONT)SelectObject(hdc, f);
            DWORD r = Real_GetGlyphOutlineA(hdc, uChar, fuFormat, lpgm, 
                                             cjBuffer, pvBuffer, lpmat2);
            SelectObject(hdc, old);
            return r;
        }
    }
    return Real_GetGlyphOutlineA(hdc, uChar, fuFormat, lpgm, cjBuffer, pvBuffer, lpmat2);
}
```

Additional text measurement APIs are also hooked for consistent rendering:
- `GetTextExtentPoint32A` / `GetTextExtentPoint32W`
- `GetTextExtentExPointA` / `GetTextExtentExPointW`
- `GetCharWidth32A` / `GetCharWidth32W`

---

## 0x02 Backlog Icon Fix

### Problem

After translation, the backlog character icons disappear entirely. This is because the game's `CheckIcon` function (at `0x004049D0`) compares the speaker name against a hardcoded table of **Japanese names** to determine which icon to display. When we replace names with Indonesian/English translations, the lookup fails and no icon is shown.

### Analysis

**Finding the CheckIcon function:**

1. In x32dbg, search for string references containing Japanese character names (e.g., `音夢`, `さくら`)
2. These strings are used in a comparison function at `0x004049D0`
3. The function is called from `0x00405013` (the call site in the backlog rendering code)

```
0x00405013:  E8 B8 F9 FF FF    call 0x004049D0   ; CheckIcon(name)
0x00405018:  ...                ; continues after call
```

**The comparison logic** (pseudocode from IDA):
```c
// At 0x004049D0 - CheckIcon function
int CheckIcon(const char* name) {
    if (strcmp(name, "音夢") == 0) { row = 0; col = 0; }
    else if (strcmp(name, "さくら") == 0) { row = 0; col = 1; }
    else if (strcmp(name, "美春") == 0) { row = 0; col = 2; }
    // ... more name comparisons
    else { return 0; }  // No icon found
    // Set icon position based on row/col
    return 1;
}
```

### Solution 1: Direct Memory Patching (Simple, if translated names are shorter)

If your translated names are shorter than or equal to the original Japanese names in bytes, you can directly overwrite them in the executable's memory:

1. In x32dbg → **Memory Map** → select all segments → **Ctrl+B** (Search Pattern)
2. Set code page to **Shift_JIS** and search for a character name (e.g., `音夢`)
3. Find the reference — use **Ctrl+R** to check which code references this string
4. Confirm it's used in the `CheckIcon` function at `0x4049D0`
5. Select the string data → **Ctrl+E** (Edit) → type your translated name → check **Keep Size**
6. x32dbg will auto-pad with `0x00` bytes
7. Also update the `strlen` comparison values if the name length changed

```
; Example: "音夢" (4 bytes in Shift_JIS) → "Nemu" (4 bytes in ASCII)
; Same length, no strlen adjustment needed

; Example: "シャルル" (8 bytes) → "Chiyoko" (7 bytes)  
; Need to adjust strlen comparison from 8 to 7
```

> [!WARNING]
> Be careful with `lea` instructions that compute values based on `strlen` results.
> For example: `lea edi, ds:[eax-0x07]` where `eax` = strlen result.
> If the original name was 8 bytes and you changed it to 6 bytes, change `-0x07` to `-0x05`.

### Solution 2: Call Site Hook (Recommended for DC4 — handles any name length)

This is the approach implemented in `dc4patch.cpp`. Instead of modifying the exe, we **hook the call to CheckIcon** and swap translated names with Japanese names before the function runs:

```cpp
// Name translation table
static const NameMapping g_nameTable[] = {
    {"Nemu",    "音夢"},
    {"Sakura",  "さくら"},
    {"Miharu",  "美春"},
    {"Hiyori",  "ひより"},
    {"Miu",     "未羽"},
    {"Shiina",  "詩名"},
    {"Arisu",   "有里栖"},
    {"Chiyoko", "ちよ子"},
    {"Nino",    "二乃"},
    {"Sorane",  "諳子"},
    // ... more mappings
    {nullptr, nullptr}
};
```

**Hook mechanism** — We overwrite the `CALL` instruction at `0x405013`:

```cpp
#define CALL_SITE_ADDR   0x00405013   // The call to CheckIcon
#define TARGET_FUNC_ADDR 0x004049D0   // CheckIcon function itself

// Naked wrapper function
__declspec(naked) void CallSiteWrapper() {
    __asm {
        pushad                    // Save all registers
        mov ecx, esp              // Pass register struct pointer
        push ecx
        call InspectAndFixParams  // Swap translated → Japanese name
        add esp, 4
        popad                     // Restore registers
        mov eax, TARGET_FUNC_ADDR
        jmp eax                   // Call original CheckIcon
    }
}

// Install the hook by patching the CALL instruction
static bool InstallCallWrapper() {
    DWORD oldProtect;
    VirtualProtect((LPVOID)CALL_SITE_ADDR, 5, PAGE_EXECUTE_READWRITE, &oldProtect);
    
    DWORD offset = (DWORD)CallSiteWrapper - CALL_SITE_ADDR - 5;
    BYTE* pCode = (BYTE*)CALL_SITE_ADDR;
    pCode[0] = 0xE8;                // CALL opcode
    *(DWORD*)(pCode + 1) = offset;  // Relative offset to our wrapper
    
    VirtualProtect((LPVOID)CALL_SITE_ADDR, 5, oldProtect, &oldProtect);
    FlushInstructionCache(GetCurrentProcess(), (LPCVOID)CALL_SITE_ADDR, 5);
    return true;
}
```

**InspectAndFixParams** scans the stack arguments for a translated name and swaps it:

```cpp
extern "C" void __cdecl InspectAndFixParams(Registers* regs) {
    const char* jpName = nullptr;
    
    // Check stack arguments [ESP+4], [ESP+8], [ESP+12]
    for (int i = 1; i <= 3; i++) {
        DWORD* pArg = (DWORD*)(regs->ESP + (i * 4));
        __try {
            if (IsTranslatedName(*pArg, &jpName)) {
                *pArg = (DWORD)jpName;  // Replace with Japanese name
                return;
            }
        }
        __except(EXCEPTION_EXECUTE_HANDLER) {}
    }
}
```

> [!NOTE]
> The hook is currently disabled (`BACKLOG_HOOK_ENABLED 0`) in `dc4patch.cpp` due to stability concerns.
> Set `#define BACKLOG_HOOK_ENABLED 1` to enable it, but be aware it may cause crashes —
> additional debugging of the exact register/stack layout at `0x405013` is needed.

### Solution 3: Full Hook with SetNameIconEx (DC3WY Style)

Following the DC3WY approach exactly, you can hook at the `else` branch of the name comparison chain and implement a completely custom lookup:

```cpp
// Hook at the else branch (end of name comparisons)
// Find the address in DC4 equivalent of DC3WY's 0x404BFE

__declspec(naked) auto JmpSetNameIconEx(void) -> void {
    __asm {
        sub esp, 0x08                     // Space for row & line
        mov dword ptr ss:[esp], 0x00      // int row = 0
        mov dword ptr ss:[esp+0x04], 0x00 // int line = 0
        
        lea eax, dword ptr ss:[esp]       
        push eax                          // push &row
        lea eax, dword ptr ss:[esp+0x4C]  
        push eax                          // push &line
        lea eax, dword ptr ss:[esp+0x4C]  // name pointer (adjusted for pushes)
        push eax                          // push name
        
        call SetNameIconEx
        test eax, eax
        jnz _succeed
        
        add esp, 0x08                     // Restore stack
        // ... execute original instructions ...
        mov eax, <RESUME_ADDR>
        jmp eax
    }
_succeed:
    __asm {
        mov edi, dword ptr ss:[esp]       // row → v12 (edi)
        mov eax, dword ptr ss:[esp+4]     // line
        add esp, 0x08
        mov dword ptr ss:[esp+0x10], eax  // line → v28
        mov eax, <ICON_SET_ADDR>          // Jump to icon display code
        jmp eax
    }
}

static auto __stdcall SetNameIconEx(const char* name, int& line, int& row) -> BOOL {
    // Map translated names to icon grid positions
    struct IconEntry { const char* name; int line; int row; };
    static const IconEntry entries[] = {
        {"Nemu",    0, 0},
        {"Sakura",  0, 1},
        {"Miharu",  0, 2},
        {"Hiyori",  0, 3},
        // ... etc
    };
    
    for (auto& e : entries) {
        if (strcmp(name, e.name) == 0) {
            line = e.line;
            row = e.row;
            return TRUE;
        }
    }
    return FALSE;
}
```

> [!IMPORTANT]
> To use Solution 3, you need to find the DC4-specific addresses:
> - The address equivalent to DC3WY's `0x404BFE` (else branch in CheckIcon)
> - The icon sprite sheet grid positions for each character
> - The stack variable locations for `v12` (row/edi) and `v28` (line/[esp+0x10])
> 
> Use IDA to decompile the function at `0x4049D0` and map out the exact variable layout.

---

## 0x03 DVD Verification via DLL Hook (Alternative)

Instead of patching the exe directly, you can also bypass DVD/KEY verification through the DLL hook:

### Method: Hook the MessageBox call

```cpp
static decltype(&MessageBoxA) Real_MessageBoxA = MessageBoxA;

static int WINAPI Hook_MessageBoxA(HWND hWnd, LPCSTR lpText, LPCSTR lpCaption, UINT uType) {
    // Block DVD verification dialog
    if (lpText && strstr(lpText, "DVD") != nullptr) {
        return IDOK;  // Pretend user clicked OK
    }
    // Block KEY verification dialog  
    if (lpCaption && strstr(lpCaption, "InstKey") != nullptr) {
        return IDOK;
    }
    return Real_MessageBoxA(hWnd, lpText, lpCaption, uType);
}
```

### Method: Hook DialogBoxParamA for KEY dialog

```cpp
static decltype(&DialogBoxParamA) Real_DialogBoxParamA = DialogBoxParamA;

static INT_PTR WINAPI Hook_DialogBoxParamA(HINSTANCE hInstance, LPCSTR lpTemplateName, 
    HWND hWndParent, DLGPROC lpDialogFunc, LPARAM dwInitParam) {
    // Skip the serial key dialog entirely
    // Return a success value (1 = IDOK)
    return IDOK;
}
```

> [!CAUTION]
> The `DialogBoxParamA` hook above is aggressive — it blocks **all** dialogs.
> A safer approach is to check the dialog template/resource ID to only skip the key dialog:
> ```cpp
> // Only skip if template matches the key dialog resource ID
> if ((DWORD)lpTemplateName == KEY_DIALOG_RESOURCE_ID) {
>     return IDOK;
> }
> return Real_DialogBoxParamA(hInstance, lpTemplateName, hWndParent, lpDialogFunc, dwInitParam);
> ```

### Integration with DC4Patch.dll

Add these hooks to the `InitPatch()` function in `dc4patch.cpp`:

```cpp
static void InitPatch() {
    g_fontManager.Init();
    
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    
    // Existing hooks
    DetourAttach(&(PVOID&)Real_GetGlyphOutlineA, Hook_GetGlyphOutlineA);
    DetourAttach(&(PVOID&)Real_CreateFileA, Hook_CreateFileA);
    DetourAttach(&(PVOID&)Real_CreateWindowExA, Hook_CreateWindowExA);
    // ... other existing hooks ...
    
    // NEW: DVD/KEY verification bypass
    DetourAttach(&(PVOID&)Real_MessageBoxA, Hook_MessageBoxA);
    // Or for more targeted approach:
    // DetourAttach(&(PVOID&)Real_DialogBoxParamA, Hook_DialogBoxParamA);
    
    DetourTransactionCommit();
    
    // Backlog icon fix
    if (g_enableBacklogAllIcon) {
        InstallCallWrapper();
    }
}
```

---

## Summary of DC4 Key Addresses

| Address | Description |
|:---|:---|
| `0x004049D0` | `CheckIcon` function — compares character name and sets backlog icon |
| `0x00405013` | Call site — `call 0x4049D0` (5 bytes: `E8 B8 F9 FF FF`) |
| `0x00405018` | Resume address after the CheckIcon call |

> [!NOTE]
> These addresses are for the specific DC4.EXE version analyzed. If you have a different version,
> the addresses may differ. Use the string search techniques described above to find the correct ones.

---

## Build Instructions

```batch
:: Open "x86 Native Tools Command Prompt for VS"
cd G:\DC4\DC4Patch
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A Win32
cmake --build . --config Release
:: Output: build\Release\DC4Patch.dll and DC4Launcher.exe
```

### File Structure

```
DC4/
├── DC4.EXE              # Original game (patched or unpatched)
├── DC4Launcher.exe      # Starts game with DLL injection
├── DC4Patch.dll         # The patch DLL
├── id_Data/             # Indonesian translation files
│   ├── *.mes            # Translated .mes script files
│   └── EnableBacklogAllIcon  # Flag file to enable backlog icons
└── AdvData/             # Original game data
```

---

## Credits & References

- **DC3WY Localization Notes** by [cokkeijigen](https://github.com/cokkeijigen/CircusEnginePatchs) — the reference used for this document
- **Microsoft Detours** — API hooking library
- **MesTextTool** by [cokkeijigen](https://github.com/cokkeijigen/MesTextTool) — Mes script text tool
