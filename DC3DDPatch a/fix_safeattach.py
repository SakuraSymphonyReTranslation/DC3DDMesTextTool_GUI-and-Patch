with open("e:/Games/DC3DD/DC3DDMesTextTool_GUI and Patch/DC3DDPatch a/src/dc4patch.cpp", "r", encoding="utf-8") as f:
    content = f.read()

new_safeattach = """static void SafeAttach(PVOID* ppPointer, PVOID pDetour, const char* name) {
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    LONG attachErr = DetourAttach(ppPointer, pDetour);
    LONG commitErr = DetourTransactionCommit();
    if (attachErr != 0 || commitErr != 0) {
        TraceLog("Failed to hook %s: attachErr=%ld commitErr=%ld", name, attachErr, commitErr);
    }
}"""

import re
content = re.sub(r'static void SafeAttach\(PVOID\* ppPointer, PVOID pDetour, const char\* name\) \{.*?\}', new_safeattach, content, flags=re.DOTALL)

with open("e:/Games/DC3DD/DC3DDMesTextTool_GUI and Patch/DC3DDPatch a/src/dc4patch.cpp", "w", encoding="utf-8") as f:
    f.write(content)
print("SafeAttach updated.")
