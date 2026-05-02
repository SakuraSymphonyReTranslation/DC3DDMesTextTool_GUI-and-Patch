"""
Patch dc4patch.cpp to add TextOutA and TextOutW hooks.
This ensures that if the engine uses TextOut instead of ExtTextOut, the underscores are still filtered to spaces.
"""
import shutil

TARGET = 'dc4patch.cpp'
with open(TARGET, 'rb') as f:
    content = f.read()

shutil.copy2(TARGET, TARGET + '.bak2')

# 1. Add Hook_TextOutA and Hook_TextOutW right before Hook_GetTextExtentPoint32A
hook_code = b"""
static decltype(&TextOutA) Real_TextOutA = TextOutA;
static BOOL WINAPI Hook_TextOutA(HDC hdc, int x, int y, LPCSTR lpString, int c) {
  int actualLen = lstrlenA(lpString);
  if (c == 0 && lpString && lpString[0] != 0) {
    c = actualLen;
  } else if (c > 0 && actualLen > c && actualLen < c + 150) {
    c = actualLen;
  }

  UINT renderLen = c;
  bool performTranslation = !(g_inBacklogRender && g_fontManager.GetDisableBacklogTranslation());
  LPCSTR renderStr = FilterSjisString(lpString, &renderLen, performTranslation, true);

  return Real_TextOutA(hdc, x, y, renderStr, renderLen);
}

static decltype(&TextOutW) Real_TextOutW = TextOutW;
static BOOL WINAPI Hook_TextOutW(HDC hdc, int x, int y, LPCWSTR lpString, int c) {
  return Real_TextOutW(hdc, x, y, lpString, c);
}

static BOOL WINAPI Hook_GetTextExtentPoint32A(
"""

old_GetText = b"static BOOL WINAPI Hook_GetTextExtentPoint32A("

if old_GetText in content and b'Hook_TextOutA' not in content:
    content = content.replace(old_GetText, hook_code.strip() + b"(\n", 1)
    print("Added TextOut hooks.")

# 2. Add DetourAttach
attach_code = b"""
  DetourAttach(&(PVOID &)Real_ExtTextOutA, Hook_ExtTextOutA);
  DetourAttach(&(PVOID &)Real_TextOutA, Hook_TextOutA);
  DetourAttach(&(PVOID &)Real_TextOutW, Hook_TextOutW);
"""
old_attach = b"  DetourAttach(&(PVOID &)Real_ExtTextOutA, Hook_ExtTextOutA);"
if old_attach in content and b'Real_TextOutA' not in content[content.find(old_attach):]:
    content = content.replace(old_attach, attach_code.strip(), 1)
    print("Added DetourAttach.")

# 3. Add DetourDetach
detach_code = b"""
  DetourDetach(&(PVOID &)Real_ExtTextOutA, Hook_ExtTextOutA);
  DetourDetach(&(PVOID &)Real_TextOutA, Hook_TextOutA);
  DetourDetach(&(PVOID &)Real_TextOutW, Hook_TextOutW);
"""
old_detach = b"  DetourDetach(&(PVOID &)Real_ExtTextOutA, Hook_ExtTextOutA);"
if old_detach in content and b'Real_TextOutA' not in content[content.find(old_detach):]:
    content = content.replace(old_detach, detach_code.strip(), 1)
    print("Added DetourDetach.")


with open(TARGET, 'wb') as f:
    f.write(content)

print("Patching complete.")
