import re

file_path = r'e:\Games\DC3DD\DC3DDMesTextTool_GUI and Patch\DC3DDPatch a\src\dc4patch.cpp'

with open(file_path, 'r', encoding='utf-8') as f:
    content = f.read()

# APIs to change from SafeAttach to PatchIAT
apis = [
    ("Real_GetACP", "Hook_GetACP", "KERNEL32.dll", "GetACP"),
    ("Real_GetOEMCP", "Hook_GetOEMCP", "KERNEL32.dll", "GetOEMCP"),
    ("Real_GetGlyphOutlineW", "Hook_GetGlyphOutlineW", "GDI32.dll", "GetGlyphOutlineW"),
    ("Real_GetGlyphOutlineA", "Hook_GetGlyphOutlineA", "GDI32.dll", "GetGlyphOutlineA"),
    ("Real_CreateFileA", "Hook_CreateFileA", "KERNEL32.dll", "CreateFileA"),
    ("Real_CreateFileW", "Hook_CreateFileW", "KERNEL32.dll", "CreateFileW"),
    ("Real_GetTextExtentPoint32A", "Hook_GetTextExtentPoint32A", "GDI32.dll", "GetTextExtentPoint32A"),
    ("Real_GetTextExtentPoint32W", "Hook_GetTextExtentPoint32W", "GDI32.dll", "GetTextExtentPoint32W"),
    ("Real_GetTextExtentExPointA", "Hook_GetTextExtentExPointA", "GDI32.dll", "GetTextExtentExPointA"),
    ("Real_GetTextExtentExPointW", "Hook_GetTextExtentExPointW", "GDI32.dll", "GetTextExtentExPointW"),
    ("Real_GetCharWidth32A", "Hook_GetCharWidth32A", "GDI32.dll", "GetCharWidth32A"),
    ("Real_GetCharWidth32W", "Hook_GetCharWidth32W", "GDI32.dll", "GetCharWidth32W"),
    ("Real_DrawTextA", "Hook_DrawTextA", "USER32.dll", "DrawTextA"),
    ("Real_DrawTextW", "Hook_DrawTextW", "USER32.dll", "DrawTextW"),
    ("Real_DrawTextExA", "Hook_DrawTextExA", "USER32.dll", "DrawTextExA"),
    ("Real_DrawTextExW", "Hook_DrawTextExW", "USER32.dll", "DrawTextExW"),
    ("Real_MessageBoxA", "Hook_MessageBoxA", "USER32.dll", "MessageBoxA"),
    ("Real_MessageBoxW", "Hook_MessageBoxW", "USER32.dll", "MessageBoxW"),
    ("Real_MessageBoxExA", "Hook_MessageBoxExA", "USER32.dll", "MessageBoxExA"),
    ("Real_MessageBoxExW", "Hook_MessageBoxExW", "USER32.dll", "MessageBoxExW"),
    ("Real_MessageBoxIndirectA", "Hook_MessageBoxIndirectA", "USER32.dll", "MessageBoxIndirectA"),
    ("Real_MessageBoxIndirectW", "Hook_MessageBoxIndirectW", "USER32.dll", "MessageBoxIndirectW"),
    ("Real_DialogBoxParamA", "Hook_DialogBoxParamA", "USER32.dll", "DialogBoxParamA"),
    ("Real_AppendMenuA", "Hook_AppendMenuA", "USER32.dll", "AppendMenuA"),
    ("Real_InsertMenuA", "Hook_InsertMenuA", "USER32.dll", "InsertMenuA"),
    ("Real_ModifyMenuA", "Hook_ModifyMenuA", "USER32.dll", "ModifyMenuA"),
    ("Real_AppendMenuW", "Hook_AppendMenuW", "USER32.dll", "AppendMenuW"),
    ("Real_InsertMenuW", "Hook_InsertMenuW", "USER32.dll", "InsertMenuW"),
    ("Real_ModifyMenuW", "Hook_ModifyMenuW", "USER32.dll", "ModifyMenuW"),
    ("Real_TrackPopupMenu", "Hook_TrackPopupMenu", "USER32.dll", "TrackPopupMenu"),
    ("Real_TrackPopupMenuEx", "Hook_TrackPopupMenuEx", "USER32.dll", "TrackPopupMenuEx"),
    ("Real_GetTextMetricsA", "Hook_GetTextMetricsA", "GDI32.dll", "GetTextMetricsA"),
    ("Real_CreateWindowExA", "Hook_CreateWindowExA", "USER32.dll", "CreateWindowExA"),
    ("Real_CreateWindowExW", "Hook_CreateWindowExW", "USER32.dll", "CreateWindowExW"),
    ("Real_SetWindowTextA", "Hook_SetWindowTextA", "USER32.dll", "SetWindowTextA"),
    ("Real_SetWindowTextW", "Hook_SetWindowTextW", "USER32.dll", "SetWindowTextW"),
    ("Real_ExtTextOutA", "Hook_ExtTextOutA", "GDI32.dll", "ExtTextOutA"),
    ("Real_ExtTextOutW", "Hook_ExtTextOutW", "GDI32.dll", "ExtTextOutW"),
    ("Real_TextOutA", "Hook_TextOutA", "GDI32.dll", "TextOutA"),
    ("Real_TextOutW", "Hook_TextOutW", "GDI32.dll", "TextOutW"),
]

patch_iat_code = "  // IAT hooks to bypass Locale Emulator inline hook conflicts\n  HMODULE hMain = GetModuleHandleA(NULL);\n"
for real, hook, dll, func in apis:
    # Comment out SafeAttach
    content = re.sub(rf'^\s*SafeAttach\(&\(PVOID &\){real},\s*[^,]+,\s*"[^"]+"\);\s*$', f'  // SafeAttach disabled for IAT -> {real}', content, flags=re.MULTILINE)
    content = re.sub(rf'^\s*SafeAttach\(&\(PVOID &\){real},\s*[^)]+\);\s*$', f'  // SafeAttach disabled for IAT -> {real}', content, flags=re.MULTILINE)
    
    # Add to PatchIAT block
    patch_iat_code += f'  PatchIAT(hMain, "{dll}", (PROC){func}, (PROC){hook});\n'

# Find the end of InitPatch and insert the PatchIAT code
content = re.sub(r'  bool iatCWa = PatchIAT.*?bool iatCFIW = PatchIAT.*?\n', '', content, flags=re.DOTALL)
content = content.replace("  TraceLog(\"InitPatch end\");", patch_iat_code + "\n  TraceLog(\"InitPatch end\");")

with open(file_path, 'w', encoding='utf-8') as f:
    f.write(content)

print("Patch applied!")
