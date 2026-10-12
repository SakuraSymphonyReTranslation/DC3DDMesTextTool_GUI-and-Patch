#pragma once
#include "dc3dd_backlog.h"

// Verified against DC3DD.exe SHA256 d2a26be40e50b54b465bb8f907e0751969d057e7b2c626c710ac8c8977733384.
// Included after FontManager, MemWrite and JmpWrite in dc3ddpatch.cpp.
namespace dc3dd_layout {
static thread_local int bodySpacing = 0;
static bool installed = false;

inline bool HasRuby(const char *text) {
  if (!text) return false;
  const unsigned char *p = reinterpret_cast<const unsigned char *>(text);
  for (; *p; ++p) {
    if (*p == 0x81 && p[1] == 0x6F) return true;
    if ((*p >= 0x81 && *p <= 0x9F) || (*p >= 0xE0 && *p <= 0xFC)) {
      if (!p[1]) break;
      ++p;
    }
  }
  return false;
}

// These wrappers replace only verified body CALL instructions. Names, UI text,
// and explicit engine line pitch ([this+0x78]) never enter the spacing hook.
int __fastcall DialogueText(void *self, void *, char *text) {
  const int saved = bodySpacing;
  bodySpacing = (!g_fontManager.GetDisableBacklogSpacing() &&
                 g_fontManager.GetAdvancedSettings() && !HasRuby(text))
                    ? g_fontManager.GetDialogueLineSpacing() : 0;
  const int result = reinterpret_cast<int(__thiscall *)(void *, char *)>(
      0x0043FA40)(self, text);
  bodySpacing = saved;
  return result;
}

int __fastcall BacklogText(void *self, void *, int x, int y, int color,
                           char *text, int a6, int a7, int a8) {
  const int saved = bodySpacing;
  bodySpacing = (!g_fontManager.GetDisableBacklogSpacing() &&
                 g_fontManager.GetAdvancedSettings() && !HasRuby(text))
                    ? g_fontManager.GetBacklogLineSpacing() : 0;
  const int result = reinterpret_cast<int(__thiscall *)(void *, int, int, int,
      char *, int, int, int)>(0x00417C70)(self, x, y, color, text, a6, a7, a8);
  bodySpacing = saved;
  return result;
}

__declspec(noinline) int __stdcall LineAdvance(int height) {
  if (height < 12) return height;
  int spacing = bodySpacing;
  if (spacing < -10) spacing = -10;
  if (spacing > 64) spacing = 64;
  return height + spacing;
}

__declspec(naked) void LineAdvanceHook() {
  __asm {
    mov ecx, [esp+24h]
    pushfd
    pushad
    push ecx
    call LineAdvance
    mov [esp+18h], eax
    popad
    popfd
    add [esp+10h], ecx
    push 0043F993h
    ret
  }
}

int __stdcall BacklogIcon(char *name, char *scenario) {
  const auto original = reinterpret_cast<int(__stdcall *)(char *, char *)>(0x00401590);
  const char *japanese = g_fontManager.GetEnableBacklogAllIcon()
                            ? dc3dd_backlog::ResolveName(name) : nullptr;
  if (!japanese) return original(name, scenario);
  // Native Sakura variants may rewrite their input. Keep translated text intact.
  char lookup[32] = {};
  for (unsigned int i = 0; i < sizeof(lookup)-1 && japanese[i]; ++i)
    lookup[i] = japanese[i];
  return original(lookup, scenario);
}

struct Site {
  DWORD address;
  DWORD originalTarget; // zero denotes the line-advance instruction block
  void *replacement;
  BYTE saved[8];
  size_t size;
};
static Site sites[] = {
    {0x00405B19, 0x00401590, (void *)BacklogIcon, {}, 5},
    {0x0042BD79, 0x0043FA40, (void *)DialogueText, {}, 5},
    {0x0043243E, 0x0043FA40, (void *)DialogueText, {}, 5},
    {0x00405CD8, 0x00417C70, (void *)BacklogText, {}, 5},
    {0x00405CFB, 0x00417C70, (void *)BacklogText, {}, 5},
    {0x00405D1E, 0x00417C70, (void *)BacklogText, {}, 5},
    {0x0043F98B, 0, (void *)LineAdvanceHook, {}, 8},
};
static constexpr BYTE lineBytes[] = {0x8B,0x4C,0x24,0x24,0x01,0x4C,0x24,0x10};

bool Install() {
  if (installed) return true;
  if ((uintptr_t)GetModuleHandleW(nullptr) != 0x00400000) return false;
  static constexpr BYTE iconStart[] = {0x56,0x8B,0x74,0x24,0x08,0x57,0x68,0xB0,0x45,0x48,0x00};
  static constexpr BYTE setStart[] = {0x81,0xEC,0x08,0x01,0x00,0x00};
  static constexpr BYTE addStart[] = {0x6A,0xFF,0x68,0xCE,0x2C,0x48,0x00};
  if (memcmp((void *)0x401590, iconStart, sizeof(iconStart)) ||
      memcmp((void *)0x43FA40, setStart, sizeof(setStart)) ||
      memcmp((void *)0x417C70, addStart, sizeof(addStart))) return false;
  // Validate every location before changing any code.
  for (auto &site : sites) {
    memcpy(site.saved, (void *)site.address, site.size);
    if (site.originalTarget) {
      if (site.saved[0] != 0xE8 ||
          site.address + 5 + *(DWORD *)(site.saved+1) != site.originalTarget)
        return false;
    } else if (memcmp(site.saved, lineBytes, sizeof(lineBytes))) return false;
  }
  size_t written = 0;
  for (auto &site : sites) {
    BYTE code[8] = {0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90};
    code[0] = site.originalTarget ? 0xE8 : 0xE9;
    *(DWORD *)(code+1) = (DWORD)site.replacement - site.address - 5;
    if (!MemWrite(site.address, code, site.size)) {
      for (size_t i = 0; i <= written; ++i)
        MemWrite(sites[i].address, sites[i].saved, sites[i].size);
      return false;
    }
    ++written;
  }
  installed = true;
  return true;
}

void Uninstall() {
  if (!installed) return;
  for (const auto &site : sites) MemWrite(site.address, site.saved, site.size);
  installed = false;
}
} // namespace dc3dd_layout
