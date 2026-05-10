import re

with open("e:/Games/DC3DD/DC3DDMesTextTool_GUI and Patch/DC3DDPatch a/src/dc4patch.cpp", "r", encoding="utf-8") as f:
    content = f.read()

# We can just replace all DetourAttach occurrences inside InitPatch.
# Find the start of InitPatch
init_idx = content.find("static void InitPatch()")

# Define the wrapper
wrapper = """
static void SafeAttach(PVOID* ppPointer, PVOID pDetour, const char* name) {
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourAttach(ppPointer, pDetour);
    LONG err = DetourTransactionCommit();
    if (err != 0) TraceLog("Failed to hook %s: err=%ld", name, err);
}
"""

if "static void SafeAttach" not in content:
    content = content.replace("static void InitPatch() {", wrapper + "\nstatic void InitPatch() {")

# Replace all DetourAttach inside InitPatch with SafeAttach
# DetourAttach(&(PVOID &)Real_CheckIcon, Hook_CheckIcon); -> SafeAttach(&(PVOID &)Real_CheckIcon, (PVOID)Hook_CheckIcon, "Real_CheckIcon");
content = re.sub(r'DetourAttach\(&\(PVOID &\)(Real_[A-Za-z0-9_]+),\s*([^)]+)\);', r'SafeAttach(&(PVOID &)\1, (PVOID)\2, "\1");', content)
content = re.sub(r'DetourAttach\(&\(PVOID &\)(GetCharWidth[A-Za-z0-9_]+),\s*([^)]+)\);', r'SafeAttach(&(PVOID &)\1, (PVOID)\2, "\1");', content)

# Remove the outer DetourTransactionBegin and Commit around them in InitPatch
content = re.sub(r'DetourTransactionBegin\(\);\s*DetourUpdateThread\(GetCurrentThread\(\)\);', '// removed bulk tx begin', content)
content = re.sub(r'LONG detour1 = DetourTransactionCommit\(\);', 'LONG detour1 = 0;', content)
content = re.sub(r'LONG detour2 = DetourTransactionCommit\(\);', 'LONG detour2 = 0;', content)

with open("e:/Games/DC3DD/DC3DDMesTextTool_GUI and Patch/DC3DDPatch a/src/dc4patch.cpp", "w", encoding="utf-8") as f:
    f.write(content)
print("SafeAttach applied.")
