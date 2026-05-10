#include <windows.h>
/*
 * DC3DD Patch DLL (Injection Version)
 *
 * This DLL is injected by the launcher - no proxy exports needed!
 * Much simpler and more reliable.
 */

#include <commdlg.h>
#include <cctype>
#include <cstdio>
#include <cstdarg>
#include <detours.h>
#include <map>
#include <string>
#include <string_view>
#include <vector>

// ============================================================================
// BitBlt Hook - Automatic Backlog Detection
// ============================================================================
extern volatile bool g_inBacklogRender;





// ============================================================================
// Backlog Icon Fix Configuration
// ============================================================================

// ============================================================================
// Name Translation Table (Translated Given Name -> Japanese Given Name)
// ============================================================================
// The game's CheckIcon function looks up Japanese names.
// We map translated names back to Japanese for lookup.

struct NameMapping {
  const char *translated; // e.g., "Nemu"
  const char *japanese;   // e.g., "音夢"
};

static const NameMapping g_nameTable[] = {
    // Main heroines (DC3DD)
    {"Himeno", "\xe5\xa7\xab\xe4\xb9\x83"},         // 姫乃
    {"Aoi", "\xe8\x91\xb5"},                          // 葵
    {"Rikka", "\xe7\xab\x8b\xe5\xa4\x8f"},           // 立夏
    {"Sara", "\xe3\x81\x95\xe3\x82\x89"},             // さら
    {"Charles", "\xe3\x82\xb7\xe3\x83\xa3\xe3\x83\xab\xe3\x83\xab"}, // シャルル
    {"Minatsu", "\xe7\xbe\x8e\xe5\xa4\x8f"},          // 美夏
    
    // Supporting characters
    {"Kousuke", "\xe8\x80\x95\xe5\x8a\xa9"},          // 耕助
    {"Mikoto", "\xe7\xbe\x8e\xe7\x90\xb4"},           // 美琴
    {"Ruru", "\xe3\x82\x8b\xe3\x82\x8b"},             // るる
    {"Sumomo", "\xe3\x81\x99\xe3\x82\x82\xe3\x82\x82"}, // すもも
    {"Shiki", "\xe5\x9b\x9b\xe5\xad\xa3"},            // 四季
    {"Ricca", "\xe7\xab\x8b\xe5\xa4\x8f"},            // 立夏 (alternate)
    {"Suginami", "\xe6\x9d\x89\xe4\xb8\xa6"},         // 杉並
    {"Kiyotaka", "\xe6\xb8\x85\xe9\x9a\x86"},         // 清隆
    
    {nullptr, nullptr} // End marker
};

// Find Japanese name from translated name
static const char *FindJapaneseName(const char *translatedName) {
  for (int i = 0; g_nameTable[i].translated != nullptr; i++) {
    if (strcmp(g_nameTable[i].translated, translatedName) == 0) {
      return g_nameTable[i].japanese;
    }
  }
  return nullptr; // Not found
}

// ============================================================================
// Font Manager
// ============================================================================

static decltype(&CreateFontW) Real_CreateFontW = CreateFontW;
class FontManager {
private:
  std::map<int, HFONT> m_dialogueFonts;
  std::map<int, HFONT> m_backlogFonts;
  std::map<int, HFONT> m_backlogNameFonts;
  std::wstring m_dialogueFontName = L"ＭＳ ゴシック";
  std::wstring m_backlogFontName = L"ＭＳ ゴシック";
  std::wstring m_backlogNameFontName = L"ＭＳ ゴシック";
  int m_dialogueFontSizeOverride = -19;
  int m_backlogFontSizeOverride = -19;
  int m_dialogueLineSpacing = 0;
  int m_dialogueXOffset = 0;
  int m_dialogueYOffset = 0;
  int m_backlogNameFontSizeOverride = -11;
  int m_backlogXOffset = 0;
  int m_backlogLineSpacing = 0;
  int m_backlogYOffset = 12;
  int m_backlogNameXOffset = 13;
  int m_backlogNameYOffset = 0;
  int m_backlogNameSpacing = 0;
  int m_backlogDialogSpacing = 0;
  bool m_advancedSettings = true;
  bool m_enableBacklogAllIcon = true;
  bool m_enableFileRedirection = true;
  bool m_disableBacklogFont = false;
  bool m_disableBacklogSpacing = false;
  bool m_disableBacklogTranslation = false;
  std::wstring m_iniPath;
  int m_language = 0; // 0 = Japanese, 1 = id_Data (Indonesian), 2 = eng_data (English)
  DWORD m_addrCheckIcon = 0x00432010;
  DWORD m_addrCheckIconConfig = 0x004256A0;
  DWORD m_addrBacklogIconHandler = 0x004325D0;
  // Scenario title translations: SJIS Japanese key -> SJIS translated value
  std::map<std::string, std::string> m_scenarioTitles;

public:
  void Init() {
    // Determine INI path - stored in main game folder.
    WCHAR modulePath[MAX_PATH];
    GetModuleFileNameW(NULL, modulePath, MAX_PATH);
    std::wstring exeDir = modulePath;
    size_t pos = exeDir.find_last_of(L"\\/");
    if (pos != std::wstring::npos) {
      exeDir = exeDir.substr(0, pos);
    }
    m_iniPath = exeDir + L"\\DC3DDPatch.ini";
    const std::wstring legacyIniPath = exeDir + L"\\DC3PPPatch.ini";

    // Migrate legacy INI name if needed.
    if (GetFileAttributesW(m_iniPath.c_str()) == INVALID_FILE_ATTRIBUTES &&
        GetFileAttributesW(legacyIniPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
      MoveFileExW(legacyIniPath.c_str(), m_iniPath.c_str(),
                  MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED);
    }

    // Ensure savegame directory exists (no prompt).
    CreateDirectoryW((exeDir + L"\\AdvData").c_str(), NULL);
    CreateDirectoryW((exeDir + L"\\AdvData\\Savegame").c_str(), NULL);

    // Create subfolder structure for file redirection
    // id_Data subfolders
    CreateDirectoryW((exeDir + L"\\id_Data").c_str(), NULL);
    CreateDirectoryW((exeDir + L"\\id_Data\\MES").c_str(), NULL);
    CreateDirectoryW((exeDir + L"\\id_Data\\GRP").c_str(), NULL);
    CreateDirectoryW((exeDir + L"\\id_Data\\MOVIE").c_str(), NULL);
    // eng_data subfolders
    CreateDirectoryW((exeDir + L"\\eng_data").c_str(), NULL);
    CreateDirectoryW((exeDir + L"\\eng_data\\MES").c_str(), NULL);
    CreateDirectoryW((exeDir + L"\\eng_data\\GRP").c_str(), NULL);
    CreateDirectoryW((exeDir + L"\\eng_data\\MOVIE").c_str(), NULL);

    // Check if INI exists, otherwise create it with defaults
    if (GetFileAttributesW(m_iniPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
      // Write the exact requested default settings in pure Shift-JIS (ANSI)
      // This prevents WritePrivateProfileStringW from destroying Japanese characters
      // on systems with English/non-JP locales.
      HANDLE hFile = CreateFileW(m_iniPath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
      if (hFile != INVALID_HANDLE_VALUE) {
        const char* defaultIni = 
            "[Fonts]\r\n"
            "BacklogFont=MS Gothic\r\n"
            "BacklogSize=-19\r\n"
            "DialogueFont=MS Gothic\r\n"
            "DialogueSize=-19\r\n"
            "DialogueXOffset=0\r\n"
            "DialogueYOffset=0\r\n"
            "BacklogNameFont=MS Gothic\r\n"
            "BacklogNameSize=-11\r\n"
            "BacklogXOffset=0\r\n"
            "BacklogLineSpacing=0\r\n"
            "BacklogYOffset=12\r\n"
            "BacklogNameXOffset=13\r\n"
            "BacklogNameYOffset=0\r\n"
            "BacklogNameSpacing=0\r\n"
            "BacklogDialogSpacing=0\r\n"
            "AdvancedSettings=1\r\n"
            "\r\n"
            "[Settings]\r\n"
            "ShowBacklogIcon=1\r\n"
            "EnableFileRedirection=1\r\n"
            "Language=1\r\n"
            "DisableBacklogFont=0\r\n"
            "DisableBacklogSpacing=0\r\n"
            "DisableBacklogTranslation=0\r\n"
            "\r\n"
            "[Offsets]\r\n"
            "CheckIcon=0x00432010\r\n"
            "CheckIconConfig=0x004256A0\r\n"
            "BacklogIconHandler=0x004325D0\r\n";
            
        DWORD bytesWritten;
        WriteFile(hFile, defaultIni, (DWORD)strlen(defaultIni), &bytesWritten, NULL);
        CloseHandle(hFile);
      }
    }

    // Load scenario title translations from scenario_titles.ini
    {
      std::wstring titlesPath = exeDir + L"\\scenario_titles.ini";
      // Read all keys from [ScenarioTitles] section
      WCHAR keysBuf[32768];
      DWORD keysLen = GetPrivateProfileStringW(
          L"ScenarioTitles", NULL, L"", keysBuf, sizeof(keysBuf)/sizeof(WCHAR),
          titlesPath.c_str());
      if (keysLen > 0) {
        WCHAR valueBuf[512];
        LPCWSTR pKey = keysBuf;
        while (*pKey) {
          // Read value for this key
          GetPrivateProfileStringW(L"ScenarioTitles", pKey, L"", valueBuf, 512,
                                   titlesPath.c_str());
          // Convert key (JP title) to SJIS for matching
          char sjisKey[256];
          int keyLen = WideCharToMultiByte(932, 0, pKey, -1, sjisKey, sizeof(sjisKey), NULL, NULL);
          // Convert value (translated title) to SJIS for rendering
          char sjisVal[256];
          int valLen = WideCharToMultiByte(932, 0, valueBuf, -1, sjisVal, sizeof(sjisVal), NULL, NULL);
          if (keyLen > 0 && valLen > 0) {
            std::string k(sjisKey);
            std::string v(sjisVal);
            if (k != v) { // Only store if translation differs from original
              m_scenarioTitles[k] = v;
            }
          }
          pKey += wcslen(pKey) + 1; // Move to next key
        }
      }

    }
    // Patch scenario title strings directly in EXE memory
    if (!m_scenarioTitles.empty()) {
      HMODULE exeModule = GetModuleHandleW(NULL);
      if (exeModule) {
        BYTE* base = (BYTE*)exeModule;
        IMAGE_DOS_HEADER* dosH = (IMAGE_DOS_HEADER*)base;
        IMAGE_NT_HEADERS* ntH = (IMAGE_NT_HEADERS*)(base + dosH->e_lfanew);
        DWORD moduleSize = ntH->OptionalHeader.SizeOfImage;



        int patched = 0, patchedPtr = 0, notFound = 0, failed = 0;

        for (auto& kv : m_scenarioTitles) {
          const std::string& sjisKey = kv.first;
          const std::string& sjisVal = kv.second;
          size_t keyLen = sjisKey.length();

          bool found = false;
          for (DWORD off = 0x1000; off < moduleSize - keyLen - 1; off++) {
            if (memcmp(base + off, sjisKey.c_str(), keyLen + 1) == 0) {
              found = true;
              DWORD slotEnd = (DWORD)(off + keyLen + 1);
              while (slotEnd < moduleSize && base[slotEnd] == 0) slotEnd++;
              DWORD space = slotEnd - off - 1;

              if (sjisVal.length() <= space) {
                // Fits in-place
                DWORD oldProt;
                VirtualProtect(base + off, space + 1, PAGE_READWRITE, &oldProt);
                memset(base + off, 0, space + 1);
                memcpy(base + off, sjisVal.c_str(), sjisVal.length());
                VirtualProtect(base + off, space + 1, oldProt, &oldProt);
                patched++;
              } else {
                // Too long - find pointer to this string and redirect it
                DWORD_PTR strAddr = (DWORD_PTR)(base + off);
                // Allocate new buffer for translated string
                char* newBuf = (char*)VirtualAlloc(NULL, sjisVal.length() + 1, 
                                                    MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
                if (newBuf) {
                  memcpy(newBuf, sjisVal.c_str(), sjisVal.length() + 1);
                  
                  // Scan for 4-byte pointer to original string in EXE memory
                  bool ptrFound = false;
                  for (DWORD pOff = 0x1000; pOff < moduleSize - 4; pOff += 1) {
                    DWORD_PTR* ptrSlot = (DWORD_PTR*)(base + pOff);
                    if (*ptrSlot == strAddr) {
                      DWORD oldProt;
                      VirtualProtect(ptrSlot, sizeof(DWORD_PTR), PAGE_READWRITE, &oldProt);
                      *ptrSlot = (DWORD_PTR)newBuf;
                      VirtualProtect(ptrSlot, sizeof(DWORD_PTR), oldProt, &oldProt);
                      ptrFound = true;
                      patchedPtr++;
                      break;
                    }
                  }
                  if (!ptrFound) {
                    VirtualFree(newBuf, 0, MEM_RELEASE);
                    failed++;
                  }
                }
              }
              break;
            }
          }
          if (!found) {
            notFound++;
          }
        }


      }
    }

    // Load custom settings if any
    WCHAR buf[128];
    if (GetPrivateProfileStringW(L"Fonts", L"DialogueFont", L"", buf, 128,
                                 m_iniPath.c_str()) > 0) {
      m_dialogueFontName = buf;
    }
    if (GetPrivateProfileStringW(L"Fonts", L"BacklogFont", L"", buf, 128,
                                 m_iniPath.c_str()) > 0) {
      m_backlogFontName = buf;
    }
    if (GetPrivateProfileStringW(L"Fonts", L"BacklogNameFont", L"", buf, 128,
                                 m_iniPath.c_str()) > 0) {
      m_backlogNameFontName = buf;
    }

    // GetPrivateProfileIntW returns 0 for negative numbers. We need a helper.
    auto ReadInt = [&](LPCWSTR key, int defVal) {
      WCHAR valBuf[64];
      if (GetPrivateProfileStringW(L"Fonts", key, L"", valBuf, 64,
                                   m_iniPath.c_str()) > 0) {
        return _wtoi(valBuf);
      }
      return defVal;
    };

    m_dialogueFontSizeOverride = ReadInt(L"DialogueSize", 19);
    m_backlogFontSizeOverride = ReadInt(L"BacklogSize", 19);
    m_dialogueLineSpacing = ReadInt(L"DialogueLineSpacing", 0);
    m_dialogueXOffset = ReadInt(L"DialogueXOffset", 0);
    m_dialogueYOffset = ReadInt(L"DialogueYOffset", 0);
    m_backlogNameFontSizeOverride = ReadInt(L"BacklogNameSize", 11);
    m_backlogXOffset = ReadInt(L"BacklogXOffset", 0);
    m_backlogLineSpacing = ReadInt(L"BacklogLineSpacing", 0);
    m_backlogYOffset = ReadInt(L"BacklogYOffset", 12);
    m_backlogNameXOffset = ReadInt(L"BacklogNameXOffset", 13);
    m_backlogNameYOffset = ReadInt(L"BacklogNameYOffset", 0);
    m_backlogNameSpacing = ReadInt(L"BacklogNameSpacing", 0);
    m_backlogDialogSpacing = ReadInt(L"BacklogDialogSpacing", 0);
    m_advancedSettings = ReadInt(L"AdvancedSettings", 1) != 0;

    auto ReadIntOther = [&](LPCWSTR section, LPCWSTR key, int defVal) {
      WCHAR valBuf[64];
      if (GetPrivateProfileStringW(section, key, L"", valBuf, 64,
                                   m_iniPath.c_str()) > 0) {
        return _wtoi(valBuf);
      }
      return defVal;
    };

    m_enableBacklogAllIcon = ReadIntOther(L"Settings", L"ShowBacklogIcon", 1) != 0;
    
    // Read legacy settings for backwards compatibility
    bool legacyIdEnabled = ReadIntOther(L"Settings", L"EnableFileRedirection", 1) != 0;
    bool legacyEngEnabled = ReadIntOther(L"Settings", L"EnableEngData", 0) != 0;
    
    // Default to Indonesian if legacy EnableFileRedirection was true, but English overrides it.
    int defaultLang = 0;
    if (legacyIdEnabled) defaultLang = 1;
    if (legacyEngEnabled) defaultLang = 2;

    m_language = ReadIntOther(L"Settings", L"Language", defaultLang);
    m_disableBacklogFont = ReadIntOther(L"Settings", L"DisableBacklogFont", 0) != 0;
    m_disableBacklogSpacing = ReadIntOther(L"Settings", L"DisableBacklogSpacing", 0) != 0;
    m_disableBacklogTranslation = ReadIntOther(L"Settings", L"DisableBacklogTranslation", 0) != 0;

    auto ReadHexAddress = [&](LPCWSTR key, DWORD defVal) -> DWORD {
      WCHAR valBuf[64];
      if (GetPrivateProfileStringW(L"Offsets", key, L"", valBuf, 64,
                                   m_iniPath.c_str()) <= 0) {
        return defVal;
      }

      wchar_t *endPtr = nullptr;
      unsigned long parsed = wcstoul(valBuf, &endPtr, 0);
      if (endPtr == valBuf || parsed == 0UL) {
        return defVal;
      }

      return static_cast<DWORD>(parsed);
    };

    m_addrCheckIcon = ReadHexAddress(L"CheckIcon", m_addrCheckIcon);
    m_addrCheckIconConfig =
        ReadHexAddress(L"CheckIconConfig", m_addrCheckIconConfig);
    m_addrBacklogIconHandler =
        ReadHexAddress(L"BacklogIconHandler", m_addrBacklogIconHandler);

    // If no custom dialogue font is set, try fallbacks
    if (m_dialogueFontName == L"ＭＳ ゴシック" || m_dialogueFontName == L"MS Gothic") {
      const wchar_t *candidates[] = {L"MS Gothic", L"ＭＳ ゴシック", L"MS PGothic",
                                     L"Yu Gothic UI", L"Tahoma"};
      for (auto name : candidates) {
        HFONT f = Real_CreateFontW(16, 0, 0, 0, FW_NORMAL, 0, 0, 0, SHIFTJIS_CHARSET,
                              OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              DEFAULT_QUALITY, DEFAULT_PITCH, name);
        if (f) {
          m_dialogueFontName = name;
          if (m_backlogFontName == L"MS Gothic" || m_backlogFontName == L"ＭＳ ゴシック") { // Sync default if unset
            m_backlogFontName = name;
          }
          if (m_backlogNameFontName == L"MS Gothic" || m_backlogNameFontName == L"ＭＳ ゴシック") { // Sync default if unset
            m_backlogNameFontName = name;
          }
          DeleteObject(f);
          break;
        }
      }
    }
  }

  HFONT GetDialogueFont(int baseSize) {
    int requestedSize =
        m_dialogueFontSizeOverride != 0 ? m_dialogueFontSizeOverride : baseSize;
    if (m_dialogueFonts.count(requestedSize))
      return m_dialogueFonts[requestedSize];

    HFONT f = Real_CreateFontW(requestedSize, 0, 0, 0, FW_NORMAL, 0, 0, 0,
                          SHIFTJIS_CHARSET, OUT_DEFAULT_PRECIS,
                          CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                          DEFAULT_PITCH, m_dialogueFontName.c_str());
    if (f)
      m_dialogueFonts[requestedSize] = f;
    return f;
  }

  HFONT GetBacklogFont(int baseSize) {
    int requestedSize =
        m_backlogFontSizeOverride != 0 ? m_backlogFontSizeOverride : baseSize;
    if (m_backlogFonts.count(requestedSize))
      return m_backlogFonts[requestedSize];

    HFONT f = Real_CreateFontW(requestedSize, 0, 0, 0, FW_NORMAL, 0, 0, 0,
                          SHIFTJIS_CHARSET, OUT_DEFAULT_PRECIS,
                          CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                          DEFAULT_PITCH, m_backlogFontName.c_str());
    if (f)
      m_backlogFonts[requestedSize] = f;
    return f;
  }

  HFONT GetBacklogNameFont(int baseSize) {
    int requestedSize = m_backlogNameFontSizeOverride != 0
                            ? m_backlogNameFontSizeOverride
                            : baseSize;
    if (requestedSize == 0)
      requestedSize = baseSize; // Fallback
    if (m_backlogNameFonts.count(requestedSize))
      return m_backlogNameFonts[requestedSize];

    HFONT f = Real_CreateFontW(requestedSize, 0, 0, 0, FW_NORMAL, 0, 0, 0,
                          SHIFTJIS_CHARSET, OUT_DEFAULT_PRECIS,
                          CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                          DEFAULT_PITCH, m_backlogNameFontName.c_str());
    if (f)
      m_backlogNameFonts[requestedSize] = f;
    return f;
  }

  void SetDialogueFont(const std::wstring &name, int size) {
    m_dialogueFontName = name;
    m_dialogueFontSizeOverride = size;
    for (auto &p : m_dialogueFonts)
      DeleteObject(p.second);
    m_dialogueFonts.clear();

    // Save
    WritePrivateProfileStringW(L"Fonts", L"DialogueFont", name.c_str(),
                               m_iniPath.c_str());
    WritePrivateProfileStringW(L"Fonts", L"DialogueSize",
                               std::to_wstring(size).c_str(),
                               m_iniPath.c_str());
  }

  void SetBacklogFont(const std::wstring &name, int size) {
    m_backlogFontName = name;
    m_backlogFontSizeOverride = size;
    for (auto &p : m_backlogFonts)
      DeleteObject(p.second);
    m_backlogFonts.clear();

    // Save
    WritePrivateProfileStringW(L"Fonts", L"BacklogFont", name.c_str(),
                               m_iniPath.c_str());
    WritePrivateProfileStringW(L"Fonts", L"BacklogSize",
                               std::to_wstring(size).c_str(),
                               m_iniPath.c_str());
  }

  void SetBacklogNameFont(const std::wstring &name, int size) {
    m_backlogNameFontName = name;
    m_backlogNameFontSizeOverride = size;
    for (auto &p : m_backlogNameFonts)
      DeleteObject(p.second);
    m_backlogNameFonts.clear();

    // Save
    WritePrivateProfileStringW(L"Fonts", L"BacklogNameFont", name.c_str(),
                               m_iniPath.c_str());
    WritePrivateProfileStringW(L"Fonts", L"BacklogNameSize",
                               std::to_wstring(size).c_str(),
                               m_iniPath.c_str());
  }

  void SetBacklogOffsets(int xOffset, int yOffset, int spacing, int nameXOffset, int nameYOffset, int nameSpacing, int dialogSpacing, int diagSpacing) {
    m_backlogXOffset = xOffset;
    m_backlogYOffset = yOffset;
    m_backlogLineSpacing = spacing;
    m_backlogNameXOffset = nameXOffset;
    m_backlogNameYOffset = nameYOffset;
    m_backlogNameSpacing = nameSpacing;
    m_backlogDialogSpacing = dialogSpacing;
    m_dialogueLineSpacing = diagSpacing;
    WritePrivateProfileStringW(L"Fonts", L"BacklogXOffset", std::to_wstring(xOffset).c_str(), m_iniPath.c_str());
    WritePrivateProfileStringW(L"Fonts", L"BacklogYOffset", std::to_wstring(yOffset).c_str(), m_iniPath.c_str());
    WritePrivateProfileStringW(L"Fonts", L"BacklogLineSpacing", std::to_wstring(spacing).c_str(), m_iniPath.c_str());
    WritePrivateProfileStringW(L"Fonts", L"BacklogNameXOffset", std::to_wstring(nameXOffset).c_str(), m_iniPath.c_str());
    WritePrivateProfileStringW(L"Fonts", L"BacklogNameYOffset", std::to_wstring(nameYOffset).c_str(), m_iniPath.c_str());
    WritePrivateProfileStringW(L"Fonts", L"BacklogNameSpacing", std::to_wstring(nameSpacing).c_str(), m_iniPath.c_str());
    WritePrivateProfileStringW(L"Fonts", L"BacklogDialogSpacing", std::to_wstring(dialogSpacing).c_str(), m_iniPath.c_str());
    WritePrivateProfileStringW(L"Fonts", L"DialogueLineSpacing", std::to_wstring(m_dialogueLineSpacing).c_str(), m_iniPath.c_str());
  }

  void SetDialogueOffsets(int xOffset, int yOffset) {
    m_dialogueXOffset = xOffset;
    m_dialogueYOffset = yOffset;
    WritePrivateProfileStringW(L"Fonts", L"DialogueXOffset", std::to_wstring(xOffset).c_str(), m_iniPath.c_str());
    WritePrivateProfileStringW(L"Fonts", L"DialogueYOffset", std::to_wstring(yOffset).c_str(), m_iniPath.c_str());
  }

  std::wstring GetDialogueFontName() const { return m_dialogueFontName; }
  std::wstring GetBacklogFontName() const { return m_backlogFontName; }
  std::wstring GetBacklogNameFontName() const { return m_backlogNameFontName; }
  int GetDialogueFontSize() const { return m_dialogueFontSizeOverride; }
  int GetDialogueLineSpacing() const { return m_advancedSettings ? m_dialogueLineSpacing : 0; }
  int GetDialogueXOffset() const { return m_advancedSettings ? m_dialogueXOffset : 0; }
  int GetDialogueYOffset() const { return m_advancedSettings ? m_dialogueYOffset : 0; }
  int GetBacklogFontSize() const { return m_backlogFontSizeOverride; }
  int GetBacklogNameFontSize() const { return m_backlogNameFontSizeOverride; }
  int GetBacklogXOffset() const {
    return m_advancedSettings ? m_backlogXOffset : 0;
  }
  int GetBacklogLineSpacing() const {
    return m_advancedSettings ? m_backlogLineSpacing : 0;
  }
  int GetBacklogYOffset() const {
    return m_advancedSettings ? m_backlogYOffset : 12;
  }
  int GetBacklogNameXOffset() const {
    return m_advancedSettings ? m_backlogNameXOffset : 13;
  }
  int GetBacklogNameYOffset() const {
    return m_advancedSettings ? m_backlogNameYOffset : 0;
  }
  int GetBacklogNameSpacing() const {
    return m_advancedSettings ? m_backlogNameSpacing : 0;
  }
  int GetBacklogDialogSpacing() const {
    return m_advancedSettings ? m_backlogDialogSpacing : 0;
  }

  void SetAdvancedSettings(bool advanced) {
    m_advancedSettings = advanced;
    WritePrivateProfileStringW(L"Fonts", L"AdvancedSettings",
                               std::to_wstring(advanced ? 1 : 0).c_str(),
                               m_iniPath.c_str());
  }

  bool GetAdvancedSettings() const { return m_advancedSettings; }
  
  void SetEnableBacklogAllIcon(bool enable) {
    m_enableBacklogAllIcon = enable;
    WritePrivateProfileStringW(L"Settings", L"ShowBacklogIcon",
                               std::to_wstring(enable ? 1 : 0).c_str(),
                               m_iniPath.c_str());
  }

  bool GetEnableBacklogAllIcon() const { return m_enableBacklogAllIcon; }
  
  void SetLanguage(int lang) {
    m_language = lang;
    WritePrivateProfileStringW(L"Settings", L"Language",
                               std::to_wstring(lang).c_str(),
                               m_iniPath.c_str());
  }

  int GetLanguage() const { return m_language; }
  DWORD GetCheckIconAddress() const { return m_addrCheckIcon; }
  DWORD GetCheckIconConfigAddress() const { return m_addrCheckIconConfig; }
  DWORD GetBacklogIconHandlerAddress() const {
    return m_addrBacklogIconHandler;
  }

  bool GetDisableBacklogFont() const { return m_disableBacklogFont; }
  bool GetDisableBacklogSpacing() const { return m_disableBacklogSpacing; }
  bool GetDisableBacklogTranslation() const { return m_disableBacklogTranslation; }

  // Lookup scenario title translation. Returns nullptr if no match.
  const char* TranslateScenarioTitle(const char* sjisStr) const {
    if (!sjisStr || m_scenarioTitles.empty()) return nullptr;
    auto it = m_scenarioTitles.find(sjisStr);
    if (it != m_scenarioTitles.end()) {
      return it->second.c_str();
    }
    return nullptr;
  }
};

static FontManager g_fontManager;
static HWND g_mainWindow = nullptr;
static constexpr UINT_PTR kCmdPatchSettings = 0x1E00;
static UINT_PTR g_cmdRestoreWindowSize = 0;
static UINT_PTR g_cmdVersionInfo = 0;
static UINT_PTR g_originalVersionCmd = 0;

static void TraceLog(const char *fmt, ...) {
  return; // Disable trace logging
  FILE *f = fopen("dc3dd_patch_trace.log", "a");
  if (!f) return;
  va_list ap;
  va_start(ap, fmt);
  vfprintf(f, fmt, ap);
  va_end(ap);
  fprintf(f, "\n");
  fclose(f);
}

// ============================================================================
// Backlog Font Handling
// ============================================================================
// DC4's backlog font is large. This section handles font rendering in the
// backlog. Font scaling is currently disabled (100% original size).

volatile bool g_inBacklogRender = false;

// Backlog font scaling function - returns original size (100%)
static int ScaleBacklogFontSize(int originalSize) {
  if (!g_inBacklogRender)
    return originalSize;
  return originalSize;
}

// ============================================================================
// File Redirection - Load from id_Data / eng_data subfolders by file type
//
// Directory layout:
//   id_Data\MES\     - translated .mes script files (Indonesian)
//   id_Data\GRP\     - translated .grp graphics files
//   id_Data\MOVIE\   - translated movie files (.mpg, .avi, .movie)
//   eng_data\MES\    - English .mes script files
//   eng_data\GRP\    - English .grp graphics files
//   eng_data\MOVIE\  - English movie files
//
// Priority: eng_data (if enabled) > id_Data > original
// ============================================================================

// Get the subfolder name for a given file extension
static const char *GetSubfolderForExt(const char *ext) {
  if (!ext) return nullptr;
  if (_stricmp(ext, ".mes") == 0) return "MES";
  if (_stricmp(ext, ".grp") == 0 || _stricmp(ext, ".crx") == 0 ||
      _stricmp(ext, ".crm") == 0) return "GRP";
  if (_stricmp(ext, ".mpg") == 0 || _stricmp(ext, ".avi") == 0 ||
      _stricmp(ext, ".movie") == 0) return "MOVIE";
  return nullptr;
}

static std::string ReplacePathA(const char *path) {
  if (!path)
    return {};

  std::string_view sv(path);

  // Don't redirect paths that are already inside our data folders
  std::string lowerPath(sv);
  for (auto& c : lowerPath) c = tolower(c);

  if (lowerPath.find("id_data") != std::string_view::npos ||
      lowerPath.find("eng_data") != std::string_view::npos ||
      lowerPath.find("sys_data") != std::string_view::npos) {
    return {};
  }

  // Extract filename (with extension) to determine the routing folder
  size_t sep = sv.find_last_of("\\/");
  std::string_view filename = (sep != std::string_view::npos)
                                  ? sv.substr(sep + 1)
                                  : sv;

  // Extract extension
  size_t dot = filename.find_last_of('.');
  const char *subfolder = nullptr;
  if (dot != std::string_view::npos) {
    std::string ext(filename.substr(dot));
    
    // Ignore graphic/animation files for dynamic redirection.
    // They are hardlink-synced to AdvData at startup, so redirecting them now 
    // causes handle conflicts and engine access violations during animations.
    if (_stricmp(ext.c_str(), ".crx") == 0 || 
        _stricmp(ext.c_str(), ".grp") == 0 || 
        _stricmp(ext.c_str(), ".crm") == 0 ||
        _stricmp(ext.c_str(), ".pck") == 0) {
      return {};
    }
    
    subfolder = GetSubfolderForExt(ext.c_str());
  }

  // 1. Strip 'AdvData\' if it exists to get the clean relative path
  std::string relPath(sv);
  size_t advPos = lowerPath.find("advdata");
  if (advPos != std::string_view::npos) {
    size_t startPos = advPos + 7;
    while (startPos < sv.length() && (sv[startPos] == '\\' || sv[startPos] == '/')) {
      startPos++;
    }
    relPath = std::string(sv.substr(startPos));
  } else if (sv.length() > 2 && sv[1] == ':') {
    // If it's an absolute path entirely outside AdvData, just use the filename to be safe
    relPath = std::string(filename);
  } else {
    // Strip leading .\ or / from flat paths
    size_t startPos = 0;
    while (startPos < sv.length() && (sv[startPos] == '.' || sv[startPos] == '\\' || sv[startPos] == '/')) {
      startPos++;
    }
    relPath = std::string(sv.substr(startPos));
  }

  // 2. Ensure relPath starts with the correct routing subfolder (e.g. "GRP")
  if (subfolder && !relPath.empty()) {
    std::string lowerRel(relPath);
    for (auto& c : lowerRel) c = tolower(c);
    
    std::string lowerSub(subfolder);
    for (auto& c : lowerSub) c = tolower(c);

    bool hasPrefix = false;
    if (lowerRel.length() >= lowerSub.length()) {
      if (lowerRel.substr(0, lowerSub.length()) == lowerSub) {
        if (lowerRel.length() == lowerSub.length() || 
            lowerRel[lowerSub.length()] == '\\' || 
            lowerRel[lowerSub.length()] == '/') {
          hasPrefix = true;
        }
      }
    }

    if (!hasPrefix) {
      relPath = std::string(subfolder) + "\\" + relPath;
    }
  }

  // Replace all forward slashes with backslashes
  for (auto& c : relPath) {
    if (c == '/') c = '\\';
  }

  // 3. Build candidate paths to try, in priority order:
  std::string candidates[6];
  int numCandidates = 0;

  int language = g_fontManager.GetLanguage();
  bool engEnabled = (language == 2);
  bool idEnabled = (language == 1);

  if (engEnabled) {
    if (!relPath.empty()) {
      candidates[numCandidates++] = std::string(".\\eng_data\\") + relPath;
    }
    if (subfolder && sep != std::string_view::npos) {
      candidates[numCandidates++] = std::string(".\\eng_data\\") + std::string(subfolder) + "\\" + std::string(filename);
    }
  }
  
  if (idEnabled) {
    if (!relPath.empty()) {
      candidates[numCandidates++] = std::string(".\\id_Data\\") + relPath;
    }
    if (subfolder && sep != std::string_view::npos) {
      candidates[numCandidates++] = std::string(".\\id_Data\\") + std::string(subfolder) + "\\" + std::string(filename);
    }
  }

  if (idEnabled) {
    if (sep != std::string_view::npos) {
      candidates[numCandidates++] = std::string(".\\id_Data\\") + std::string(filename);
    } else {
      candidates[numCandidates++] = std::string(".\\id_Data\\") + std::string(sv);
    }
  }

  for (int i = 0; i < numCandidates; i++) {
    char dbgCheck[512];
    DWORD attr = GetFileAttributesA(candidates[i].c_str());
    bool exists = (attr != INVALID_FILE_ATTRIBUTES);
    
    if (exists) {
      sprintf_s(dbgCheck, "DC3DDPatch Route Info: File=%s -> Candidate Resolved: %s\n", path, candidates[i].c_str());
      OutputDebugStringA(dbgCheck);
      return candidates[i];
    }
  }
  return {};
}

// ============================================================================
// Backlog Icon Fix - Reverse Name Translation
// ============================================================================
//
// Problem: Translated .mes files in id_Data contain Latin character names
//          (e.g., "Ichito" instead of "一登"). CheckIcon (0x4049D0) finds
//          the icon data correctly (confirmed via x32dbg), but downstream
//          rendering code can't match Latin names to Japanese icon images.
//
// Solution: Hook after the backlog icon table is built (at 0x405276 in the
//           backlog builder function at 0x404EE0). Scan the table entries
//           and replace any translated Latin names with their Japanese
//           equivalents using g_nameTable. This fixes icon lookup while
//           the display name in the backlog text comes from a separate source.
//
// Table layout at 0x4BDA00:
//   Stride: 0x544 bytes per entry
//   [+0x000]: Resource ID (e.g., "dc4_kyo20190214d") — 0x400 bytes max
//   [+0x400]: Field1 — possibly character name
//   [+0x420]: Field2 — possibly another text field
//   [+0x440]: Field3 — possibly another text field
//   Entry count stored at [ebp+0x728] in the builder function

// JmpWrite - Write a JMP instruction at a target address
static bool JmpWrite(DWORD orgAddr, void *targetFunc) {
  BYTE jmp[5];
  jmp[0] = 0xE9; // JMP opcode
  *(DWORD *)(jmp + 1) = (DWORD)targetFunc - orgAddr - 5;

  DWORD oldProtect;
  if (!VirtualProtect((LPVOID)orgAddr, 5, PAGE_EXECUTE_READWRITE,
                      &oldProtect)) {
    return false;
  }

  SIZE_T written;
  WriteProcessMemory(INVALID_HANDLE_VALUE, (LPVOID)orgAddr, jmp, 5, &written);
  VirtualProtect((LPVOID)orgAddr, 5, oldProtect, &oldProtect);
  FlushInstructionCache(GetCurrentProcess(), (LPCVOID)orgAddr, 5);

  return (written == 5);
}

// MemWrite - Write arbitrary bytes at a target address
static bool MemWrite(DWORD addr, const void *data, size_t len) {
  DWORD oldProtect;
  if (!VirtualProtect((LPVOID)addr, len, PAGE_EXECUTE_READWRITE, &oldProtect))
    return false;
  SIZE_T written;
  WriteProcessMemory(INVALID_HANDLE_VALUE, (LPVOID)addr, data, len, &written);
  VirtualProtect((LPVOID)addr, len, oldProtect, &oldProtect);
  FlushInstructionCache(GetCurrentProcess(), (LPCVOID)addr, len);
  return (written == len);
}

// Convert a UTF-8 string to Shift-JIS (codepage 932)
// Returns the number of bytes written (excluding null terminator), or 0 on
// failure
static int Utf8ToShiftJIS(const char *utf8, char *sjisOut, int sjisMaxLen) {
  // Step 1: UTF-8 → wide char (UTF-16)
  int wideLen = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
  if (wideLen <= 0)
    return 0;

  wchar_t wideBuf[64];
  if (wideLen > 64)
    return 0;
  MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wideBuf, 64);

  // Step 2: wide char → Shift-JIS (codepage 932)
  int sjisLen = WideCharToMultiByte(932, 0, wideBuf, -1, sjisOut, sjisMaxLen,
                                    nullptr, nullptr);
  if (sjisLen <= 0)
    return 0;

  return sjisLen - 1; // exclude null terminator
}

// Scan the backlog icon table: reverse-translate names AND deduplicate entries.
// Returns the new entry count (may be less than input if duplicates were
// removed).
static int __cdecl PatchBacklogIconTable(int entryCount) {
  if (entryCount <= 0)
    return entryCount;

  // Safety cap — the table only has room for this many entries
  if (entryCount > 200)
    entryCount = 200;

#ifdef GAME_DC4PH
  static const DWORD TABLE_BASE = 0x4BDAC0;
#else
  static const DWORD TABLE_BASE = 0x4BDA00;
#endif
  static const int STRIDE = 0x544;
  static const int TEXT_FIELD = 0x000; // Dialogue text offset
  static const int NAME_FIELD = 0x400; // Character name offset

  // --- Pass 1: Reverse-translate character names ---
  if (g_fontManager.GetEnableBacklogAllIcon()) {
    for (int i = 0; i < entryCount; i++) {
      BYTE *entry = (BYTE *)(TABLE_BASE + i * STRIDE);
      char *nameField = (char *)(entry + NAME_FIELD);
      if (nameField[0] == 0)
        continue;

      const char *jpNameUtf8 = FindJapaneseName(nameField);
      if (jpNameUtf8) {
        char sjisName[32];
        int sjisLen = Utf8ToShiftJIS(jpNameUtf8, sjisName, sizeof(sjisName));
        if (sjisLen > 0) {
          memcpy(nameField, sjisName, sjisLen + 1);
        }
      }
    }
  }

  // --- Pass 2: Remove consecutive duplicate entries ---
  int newCount = entryCount;
  for (int i = 0; i < newCount - 1; i++) {
    char *textA = (char *)(TABLE_BASE + i * STRIDE + TEXT_FIELD);
    char *textB = (char *)(TABLE_BASE + (i + 1) * STRIDE + TEXT_FIELD);

    // Safety: only compare if both strings are non-empty and i+1 is in bounds
    if (i + 1 >= newCount)
      break;

    if (textA[0] != 0 && textB[0] != 0 && strcmp(textA, textB) == 0) {
      for (int j = i + 1; j < newCount - 1; j++) {
        BYTE *dst = (BYTE *)(TABLE_BASE + j * STRIDE);
        BYTE *src = (BYTE *)(TABLE_BASE + (j + 1) * STRIDE);
        memcpy(dst, src, STRIDE);
      }
      memset((BYTE *)(TABLE_BASE + (newCount - 1) * STRIDE), 0, STRIDE);
      newCount--;
      i--;
    }
  }

  return newCount;
}

// Naked asm trampoline: hooks at 0x405276 (after backlog table is built)
// Original bytes at 0x405276: 33 DB 83 E8 01 (xor ebx,ebx; sub eax,1) = 5 bytes
// At this point: eax = entry count from [ebp+0x728]
//
// PatchBacklogIconTable returns the new count (after dedup).
// We write the new count to both EAX and [ebp+0x728] so the rendering
// loop uses the correct number of entries.
__declspec(naked) void HookBacklogTableBuilt() {
  __asm {
    // eax = entry count (from mov eax,[ebp+728] at 0x405270)
        pushad // save all regs (EAX at [esp+28])
        push eax // pass entry count
        call PatchBacklogIconTable // returns new count in eax
        add esp, 4
        mov [esp+28], eax           // overwrite saved EAX with new count
        // Also update [ebp+0x728] — ebp is at [esp+8] in pushad frame
        mov ecx, [esp+8] // ecx = saved EBP (backlog object)
        mov [ecx+0x728], eax // update the entry count in the object
        popad // eax now = new count

        // Execute displaced instructions
        xor ebx, ebx // original: 33 DB
        sub eax, 1 // original: 83 E8 01

    // Jump back to 0x40527B
        push 0x0040527B
        ret
  }
}

// Install the backlog icon table hook
static bool InstallBacklogIconHook() {
  // Hook at 0x405276 (DC4) or 0x4056D6 (DC4PH) in the backlog builder function
  // Displaced: xor ebx,ebx (33 DB) + sub eax,1 (83 E8 01) = 5 bytes
#ifdef GAME_DC4PH
  return JmpWrite(0x004056D6, HookBacklogTableBuilt);
#else
  return JmpWrite(0x00405276, HookBacklogTableBuilt);
#endif
}

// ============================================================================
// Backlog Icon Detection Hook
// ============================================================================
// Hook addresses default to known DC3DD offsets and can be overridden via:
// [Offsets] CheckIcon, CheckIconConfig, BacklogIconHandler in DC3DDPatch.ini
static int(__cdecl *Real_CheckIcon)(const char*) = nullptr;
static int(__cdecl *Real_CheckIconConfig)(int, int) = nullptr;
static int(__cdecl *Real_BacklogIconHandler)(const char*) = nullptr;
static bool g_attachedCheckIcon = false;
static bool g_attachedCheckIconConfig = false;
static bool g_attachedBacklogIconHandler = false;

static bool IsExecutableAddress(const void *ptr) {
  if (!ptr) {
    return false;
  }

  MEMORY_BASIC_INFORMATION mbi{};
  if (VirtualQuery(ptr, &mbi, sizeof(mbi)) == 0) {
    return false;
  }
  if (mbi.State != MEM_COMMIT) {
    return false;
  }

  DWORD protect = mbi.Protect & 0xFF;
  switch (protect) {
  case PAGE_EXECUTE:
  case PAGE_EXECUTE_READ:
  case PAGE_EXECUTE_READWRITE:
  case PAGE_EXECUTE_WRITECOPY:
    return true;
  default:
    return false;
  }
}

// Debug logging helper - writes to file for easy inspection
static void DebugLogIcon(const char* fmt, ...) {
    return; // Disable icon debugging
    static FILE* logFile = nullptr;
    if (!logFile) {
        logFile = fopen("DC3DD_icon_debug.log", "w");
        if (!logFile) return;
    }
    va_list args;
    va_start(args, fmt);
    vfprintf(logFile, fmt, args);
    va_end(args);
    fflush(logFile);
}

static int __cdecl Hook_CheckIcon(const char* name) {
    if (name) {
        DebugLogIcon("CheckIcon(\"%s\")\n", name);

        // --- Match translated Latin character names directly ---
        if (_stricmp(name, "Rikka") == 0 || _stricmp(name, "Ricca") == 0) { DebugLogIcon("  -> matched Rikka, return 0\n"); return 0; }
        if (_stricmp(name, "Charles") == 0) { DebugLogIcon("  -> matched Charles, return 1\n"); return 1; }
        if (_stricmp(name, "Himeno") == 0) { DebugLogIcon("  -> matched Himeno, return 2\n"); return 2; }
        if (_stricmp(name, "Sara") == 0) { DebugLogIcon("  -> matched Sara, return 3\n"); return 3; }
        if (_stricmp(name, "Aoi") == 0) { DebugLogIcon("  -> matched Aoi, return 4\n"); return 4; }
        if (_stricmp(name, "Minatsu") == 0) { DebugLogIcon("  -> matched Minatsu, return 5\n"); return 5; }
        if (_stricmp(name, "Sumomo") == 0) { DebugLogIcon("  -> matched Sumomo, return 6\n"); return 6; }
        if (_stricmp(name, "Ruru") == 0) { DebugLogIcon("  -> matched Ruru, return 1\n"); return 1; }
        if (_stricmp(name, "Shiki") == 0) { DebugLogIcon("  -> matched Shiki, return 7\n"); return 7; }

        // --- Parse voice code from resource path ---
        const char* p = name;
        while (*p) p++;
        while (p > name && isdigit((unsigned char)*(p - 1))) p--;
        if (p - name >= 3) {
            const char* code = p - 3;
            if (_strnicmp(code, "rcc", 3) == 0) { DebugLogIcon("  -> voice code RCC, return 0\n"); return 0; }
            if (_strnicmp(code, "srr", 3) == 0) { DebugLogIcon("  -> voice code SRR, return 1\n"); return 1; }
            if (_strnicmp(code, "hmn", 3) == 0) { DebugLogIcon("  -> voice code HMN, return 2\n"); return 2; }
            if (_strnicmp(code, "sra", 3) == 0) { DebugLogIcon("  -> voice code SRA, return 3\n"); return 3; }
            if (_strnicmp(code, "aoi", 3) == 0) { DebugLogIcon("  -> voice code AOI, return 4\n"); return 4; }
            if (_strnicmp(code, "tmj", 3) == 0) { DebugLogIcon("  -> voice code TMJ, return 5\n"); return 5; }
            if (_strnicmp(code, "smm", 3) == 0) { DebugLogIcon("  -> voice code SMM, return 6\n"); return 6; }
            if (_strnicmp(code, "yuk", 3) == 0) { DebugLogIcon("  -> voice code YUK, return 7\n"); return 7; }
        }
        DebugLogIcon("  -> no match, calling original\n");
    }
    if (!Real_CheckIcon) {
      return -1;
    }
    int result = Real_CheckIcon(name);
    DebugLogIcon("  -> original returned %d\n", result);
    return result;
}

// ============================================================================
// Icon Config Check Hook
// ============================================================================
// The game checks an array at 0x741160 to see if faces are toggled off by the user.
// Returns 0 if faces are enabled, or non-zero if disabled.

static int __cdecl Hook_CheckIconConfig(int config_id, int icon_index) {
    if (config_id == 100) {
        // ID 100 is the backlog face icon visibility check.
        // Return 0 to force face icons to be enabled!
        return 0;
    }
    if (!Real_CheckIconConfig) {
      return 0;
    }
    return Real_CheckIconConfig(config_id, icon_index);
}

// ============================================================================
// BacklogIconHandler Hook - trace if it's even called
// ============================================================================
static int __cdecl Hook_BacklogIconHandler(const char* cmd) {
    DebugLogIcon("BacklogIconHandler(\"%s\")\n", cmd ? cmd : "(null)");
    
    // Check the enable flags the original function checks
    DWORD flag1 = *(DWORD*)0x741120;
    DWORD flag2 = *(DWORD*)0xBDD7F8;
    DebugLogIcon("  [0x741120]=%u, [0xBDD7F8]=%u\n", flag1, flag2);
    
    if (!Real_BacklogIconHandler) {
      return 0;
    }
    int result = Real_BacklogIconHandler(cmd);
    DebugLogIcon("  -> BacklogIconHandler returned %d\n", result);
    return result;
}

// ============================================================================
// Hooks
// ============================================================================

static decltype(&GetGlyphOutlineA) Real_GetGlyphOutlineA = GetGlyphOutlineA;
static decltype(&CreateFileA) Real_CreateFileA = CreateFileA;
static decltype(&CreateFileW) Real_CreateFileW = CreateFileW;

static HANDLE WINAPI Hook_CreateFileW(
    LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition,
    DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) {

  static thread_local bool s_inCreateFileW = false;
  if (s_inCreateFileW) {
    return Real_CreateFileW(lpFileName, dwDesiredAccess, dwShareMode,
                            lpSecurityAttributes, dwCreationDisposition,
                            dwFlagsAndAttributes, hTemplateFile);
  }

  s_inCreateFileW = true;

  int lang = g_fontManager.GetLanguage();
  if (lang == 1 || lang == 2) {
    int len = WideCharToMultiByte(932, 0, lpFileName, -1, NULL, 0, NULL, NULL);
    if (len > 0) {
      std::string pathA(len, '\0');
      WideCharToMultiByte(932, 0, lpFileName, -1, &pathA[0], len, NULL, NULL);
      pathA.resize(len - 1);
      
      std::string newPathA = ReplacePathA(pathA.c_str());
      if (!newPathA.empty()) {
        // Resolve to absolute path
        char absPath[MAX_PATH];
        DWORD absLen = GetFullPathNameA(newPathA.c_str(), MAX_PATH, absPath, NULL);
        const char* finalPathA = (absLen > 0 && absLen < MAX_PATH) ? absPath : newPathA.c_str();

        int wlen = MultiByteToWideChar(932, 0, finalPathA, -1, NULL, 0);
        if (wlen > 0) {
          std::wstring newPathW(wlen, L'\0');
          MultiByteToWideChar(932, 0, finalPathA, -1, &newPathW[0], wlen);
          newPathW.resize(wlen - 1);
          
          char debugStr[512];
          sprintf_s(debugStr, "DC3DDPatch Redirecting W: %s -> %s\n", pathA.c_str(), finalPathA);
          OutputDebugStringA(debugStr);

          HANDLE h = Real_CreateFileW(newPathW.c_str(), dwDesiredAccess, dwShareMode,
                                      lpSecurityAttributes, dwCreationDisposition,
                                      dwFlagsAndAttributes, hTemplateFile);
          s_inCreateFileW = false;
          return h;
        }
      }
    }
  }

  HANDLE h = Real_CreateFileW(lpFileName, dwDesiredAccess, dwShareMode,
                              lpSecurityAttributes, dwCreationDisposition,
                              dwFlagsAndAttributes, hTemplateFile);
  s_inCreateFileW = false;
  return h;
}
static decltype(&CreateWindowExA) Real_CreateWindowExA = CreateWindowExA;
static decltype(&GetTextExtentPoint32A) Real_GetTextExtentPoint32A =
    GetTextExtentPoint32A;
static decltype(&GetTextExtentPoint32W) Real_GetTextExtentPoint32W =
    GetTextExtentPoint32W;
static decltype(&MessageBoxA) Real_MessageBoxA = MessageBoxA;
static decltype(&MessageBoxW) Real_MessageBoxW = MessageBoxW;
static decltype(&MessageBoxExA) Real_MessageBoxExA = MessageBoxExA;
static decltype(&MessageBoxExW) Real_MessageBoxExW = MessageBoxExW;
static decltype(&MessageBoxIndirectA) Real_MessageBoxIndirectA = MessageBoxIndirectA;
static decltype(&MessageBoxIndirectW) Real_MessageBoxIndirectW = MessageBoxIndirectW;
static decltype(&DialogBoxParamA) Real_DialogBoxParamA = DialogBoxParamA;
static decltype(&AppendMenuA) Real_AppendMenuA = AppendMenuA;
static decltype(&InsertMenuA) Real_InsertMenuA = InsertMenuA;
static decltype(&ModifyMenuA) Real_ModifyMenuA = ModifyMenuA;
static decltype(&AppendMenuW) Real_AppendMenuW = AppendMenuW;
static decltype(&InsertMenuW) Real_InsertMenuW = InsertMenuW;
static decltype(&ModifyMenuW) Real_ModifyMenuW = ModifyMenuW;
static decltype(&TrackPopupMenu) Real_TrackPopupMenu = TrackPopupMenu;
static decltype(&TrackPopupMenuEx) Real_TrackPopupMenuEx = TrackPopupMenuEx;
static decltype(&GetTextMetricsA) Real_GetTextMetricsA = GetTextMetricsA;
static decltype(&ExtTextOutA) Real_ExtTextOutA = ExtTextOutA;

// ============================================================================
// Japanese Codepage Hooks (Wine/Proton fix)
// ============================================================================
// On Wine/Proton (Android), GetACP() may return the host Linux codepage
// instead of 932 (Shift-JIS), breaking Japanese string handling in the game.
// These hooks are harmless on Windows (LE already set the codepage).
// NOTE: LCID/LangID hooks are intentionally omitted - they break backlog.

typedef UINT(WINAPI *pfnGetACP)(void);
typedef UINT(WINAPI *pfnGetOEMCP)(void);

static pfnGetACP Real_GetACP = GetACP;
static pfnGetOEMCP Real_GetOEMCP = GetOEMCP;

static UINT WINAPI Hook_GetACP() { return 932; }
static UINT WINAPI Hook_GetOEMCP() { return 932; }

// ============================================================================
// Indonesian UI Translation System
// ============================================================================
// Two-pronged approach:
// 1. Hook-based: intercepts MessageBoxA, AppendMenuA etc. for runtime
// translation
// 2. Memory-patch: scans loaded EXE's .rdata/.data and replaces strings
// in-place
//    at startup, catching chapter titles and game-engine-rendered text.
// Indonesian text is pure ASCII which is valid in Shift-JIS codepage.

struct UITranslation {
  const char *japanese;   // Original SJIS bytes
  const char *indonesian; // Indonesian replacement (ASCII)
};

// Master translation table - used by both hooks and memory patcher
static const UITranslation g_uiTranslations[] = {
    // Version dialog content - Japanese game title to Latin
    // "D.C.Ⅲ DreamDays～ダ・カーポⅢ～ドリームデイズ"
    {"D.C.\x87\x56 DreamDays\x81\x60\x83_\x81\x45\x83J\x81\x5B\x83|\x87\x56\x81\x60\x83h\x83\x8a\x81[\x83\x80\x83""f\x83""C\x83Y",
     "D.C. III ~Da Capo III~ Dream Days"},
    {nullptr, nullptr} // End marker
};
// Translate a SJIS string if it matches a known entry (exact match)
static const char *TranslateUI(const char *sjisText) {
  if (!sjisText)
    return nullptr;
  if (g_fontManager.GetLanguage() != 1)
    return nullptr;
  for (int i = 0; g_uiTranslations[i].japanese != nullptr; i++) {
    if (strcmp(sjisText, g_uiTranslations[i].japanese) == 0) {
      return g_uiTranslations[i].indonesian;
    }
  }
  return nullptr;
}

// Check if a string contains a SJIS substring and return the translation
static const char *TranslateUIPartial(const char *sjisText) {
  if (!sjisText)
    return nullptr;
  if (g_fontManager.GetLanguage() != 1)
    return nullptr;
  for (int i = 0; g_uiTranslations[i].japanese != nullptr; i++) {
    if (strstr(sjisText, g_uiTranslations[i].japanese) != nullptr) {
      return g_uiTranslations[i].indonesian;
    }
  }
  return nullptr;
}

// ============================================================================
// Memory String Patcher - patches strings directly in the loaded EXE image
// ============================================================================
// Scans .rdata and .data sections and replaces Japanese strings with
// Indonesian translations in-place. Only patches where the translation
// fits within the original string's byte space.

static void PatchStringsInMemory() {
  if (g_fontManager.GetLanguage() != 1)
    return;

  HMODULE hExe = GetModuleHandleA(NULL);
  if (!hExe)
    return;

  BYTE *base = (BYTE *)hExe;
  IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)base;
  if (dos->e_magic != IMAGE_DOS_SIGNATURE)
    return;

  IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(base + dos->e_lfanew);
  if (nt->Signature != IMAGE_NT_SIGNATURE)
    return;

  IMAGE_SECTION_HEADER *sec = IMAGE_FIRST_SECTION(nt);
  int numSections = nt->FileHeader.NumberOfSections;

  // Make all target sections writable first
  DWORD oldProtects[32] = {};
  for (int s = 0; s < numSections && s < 32; s++) {
    char secName[9] = {};
    memcpy(secName, sec[s].Name, 8);
    if (strcmp(secName, ".rdata") != 0 && strcmp(secName, ".data") != 0 &&
        strcmp(secName, ".text") != 0)
      continue;
    BYTE *secStart = base + sec[s].VirtualAddress;
    DWORD secSize = sec[s].Misc.VirtualSize;
    VirtualProtect(secStart, secSize, PAGE_READWRITE, &oldProtects[s]);
  }

  for (int i = 0; g_uiTranslations[i].japanese != nullptr; i++) {
    const char *jp = g_uiTranslations[i].japanese;
    const char *id = g_uiTranslations[i].indonesian;
    int jpLen = (int)strlen(jp);
    int idLen = (int)strlen(id);

    if (idLen <= jpLen) {
      // === PASS 1: In-place replacement (translation fits) ===
      for (int s = 0; s < numSections; s++) {
        char secName[9] = {};
        memcpy(secName, sec[s].Name, 8);
        if (strcmp(secName, ".rdata") != 0 && strcmp(secName, ".data") != 0)
          continue;

        BYTE *secStart = base + sec[s].VirtualAddress;
        DWORD secSize = sec[s].Misc.VirtualSize;

        for (DWORD offset = 0; offset + jpLen <= secSize; offset++) {
          if (memcmp(secStart + offset, jp, jpLen) == 0) {
            if (secStart[offset + jpLen] != 0)
              continue;
            memcpy(secStart + offset, id, idLen);
            memset(secStart + offset + idLen, 0, jpLen - idLen + 1);
          }
        }
      }
    } else {
      // === PASS 2: Pointer redirection (translation too long) ===
      // 1. Find address of the SJIS string in the EXE's data sections
      BYTE *strAddr = nullptr;
      for (int s = 0; s < numSections && !strAddr; s++) {
        char secName[9] = {};
        memcpy(secName, sec[s].Name, 8);
        if (strcmp(secName, ".rdata") != 0 && strcmp(secName, ".data") != 0)
          continue;

        BYTE *secStart = base + sec[s].VirtualAddress;
        DWORD secSize = sec[s].Misc.VirtualSize;

        for (DWORD offset = 0; offset + jpLen <= secSize; offset++) {
          if (memcmp(secStart + offset, jp, jpLen) == 0) {
            if (secStart[offset + jpLen] != 0)
              continue;
            strAddr = secStart + offset;
            break;
          }
        }
      }

      if (!strAddr)
        continue;

      // 2. Allocate persistent memory for the longer Indonesian string
      char *newStr = (char *)VirtualAlloc(
          NULL, idLen + 1, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
      if (!newStr)
        continue;
      memcpy(newStr, id, idLen + 1);

      // 3. Scan ALL sections for pointers (4-byte values) to the original
      // string
      //    and redirect them to our new string
      DWORD oldAddr = (DWORD)(uintptr_t)strAddr;
      DWORD newAddr = (DWORD)(uintptr_t)newStr;

      for (int s = 0; s < numSections; s++) {
        char secName[9] = {};
        memcpy(secName, sec[s].Name, 8);
        // Scan .rdata, .data, and .text for pointer references
        if (strcmp(secName, ".rdata") != 0 && strcmp(secName, ".data") != 0 &&
            strcmp(secName, ".text") != 0)
          continue;

        BYTE *secStart = base + sec[s].VirtualAddress;
        DWORD secSize = sec[s].Misc.VirtualSize;

        // Scan for 4-byte pointer values matching the original string address
        for (DWORD offset = 0; offset + 4 <= secSize; offset++) {
          DWORD val;
          memcpy(&val, secStart + offset, 4);
          if (val == oldAddr) {
            memcpy(secStart + offset, &newAddr, 4);
          }
        }
      }
    }
  }

  // Restore original protections
  for (int s = 0; s < numSections && s < 32; s++) {
    if (oldProtects[s] == 0)
      continue;
    char secName[9] = {};
    memcpy(secName, sec[s].Name, 8);
    if (strcmp(secName, ".rdata") != 0 && strcmp(secName, ".data") != 0 &&
        strcmp(secName, ".text") != 0)
      continue;
    BYTE *secStart = base + sec[s].VirtualAddress;
    DWORD secSize = sec[s].Misc.VirtualSize;
    VirtualProtect(secStart, secSize, oldProtects[s], &oldProtects[s]);
  }
}

static decltype(&MultiByteToWideChar) Real_MultiByteToWideChar = MultiByteToWideChar;
static int WINAPI Hook_MultiByteToWideChar(UINT CodePage, DWORD dwFlags, LPCCH lpMultiByteStr, int cbMultiByte, LPWSTR lpWideCharStr, int cchWideChar) {
    if (CodePage == CP_ACP || CodePage == CP_THREAD_ACP) {
        CodePage = 932; // Force Shift-JIS
    }
    int result = Real_MultiByteToWideChar(CodePage, dwFlags, lpMultiByteStr, cbMultiByte, lpWideCharStr, cchWideChar);
    
    // Intercept decoded strings and replace underscore with space.
    // This catches text converted for internal DirectX/custom font rendering.
    if (result > 0 && lpWideCharStr != NULL && cchWideChar > 0) {
        for (int i = 0; i < result; i++) {
            if (lpWideCharStr[i] == L'_' || lpWideCharStr[i] == 0xFF3F) {
                lpWideCharStr[i] = L' ';
            }
        }
    }
    return result;
}

// Hook GetTextMetricsA to reduce line height in backlog
// This is the KEY hook for line spacing - the game uses tmHeight for line
// spacing
static BOOL WINAPI Hook_GetTextMetricsA(HDC hdc, LPTEXTMETRICA lptm) {
  BOOL res = Real_GetTextMetricsA(hdc, lptm);
  if (res) {
    if (g_inBacklogRender) {
      int spacing = g_fontManager.GetBacklogLineSpacing();
      lptm->tmHeight += spacing;
      lptm->tmAscent += spacing;
    } else {
      int yOffset = g_fontManager.GetDialogueYOffset();
      // DO NOT modify tmAscent here, it causes underflow crashes! 
      // We apply DialogueYOffset in GetGlyphOutline instead.
      
      int lineSpacing = g_fontManager.GetDialogueLineSpacing();
      if (lineSpacing != 0) {
          lptm->tmHeight += lineSpacing;
          if (lptm->tmHeight < 1) lptm->tmHeight = 1;
      }
    }
  }
  return res;
}

// Translate full-width punctuation, letters, and numbers into their 1-byte ASCII equivalents
static const char* FilterSjisString(LPCSTR lpString, UINT* c_inout, bool performTranslation = true, bool replaceKatakanaDot = true) {
  if (!lpString || *c_inout == 0)
    return lpString;

  static thread_local char s_buffer[1024]; // Safe upper bound for text out calls
  UINT len = *c_inout;
  if (len >= sizeof(s_buffer)) {
    len = sizeof(s_buffer) - 1; // Truncate safely
  }

  bool modified = false;
  UINT outLen = 0;
  for (UINT i = 0; i < len; i++) {
    unsigned char ch1 = (unsigned char)lpString[i];
    
    // Check for double-byte Shift-JIS characters
    if ((ch1 >= 0x81 && ch1 <= 0x9F) || (ch1 >= 0xE0 && ch1 <= 0xFC)) {
      if (i + 1 < len) {
        unsigned char ch2 = (unsigned char)lpString[i + 1];
        char replacement = 0;
        
        if (performTranslation) {
          if (ch1 == 0x81) {
            switch (ch2) {
              case 0x40: replacement = ' '; break; // Full space -> Latin space
            case 0x41: replacement = ','; break; // 、
            case 0x42: replacement = '.'; break; // 。
            case 0x43: replacement = ','; break; // ，
            case 0x44: replacement = '.'; break; // ．
            case 0x45: replacement = 'X'; break; // ・
            case 0x46: replacement = ':'; break; // ：
            case 0x47: replacement = ';'; break; // ；
            case 0x48: replacement = '?'; break; // ？
            case 0x49: replacement = '!'; break; // ！
            case 0x5B: replacement = '-'; break; // ー
            case 0x5C: replacement = '-'; break; // ―
            case 0x5D: replacement = '-'; break; // ‐
            case 0x5E: replacement = '/'; break; // ／
            case 0x5F: replacement = '\\'; break;// ＼
            case 0x60: replacement = '~'; break; // ～
            case 0x61: replacement = '|'; break; // ∥
            case 0x62: replacement = '|'; break; // ｜
            case 0x65: replacement = '\''; break;// ‘
            case 0x66: replacement = '\''; break;// ’
            case 0x67: replacement = '\"'; break;// “
            case 0x51: replacement = 'X'; break; // ＿ (Full-width underscore)
            case 0x68: replacement = '\"'; break;// ”
            case 0x69: replacement = '('; break; // （
            case 0x6A: replacement = ')'; break; // ）
            case 0x6D: replacement = '['; break; // ［
            case 0x6E: replacement = ']'; break; // ］
            case 0x6F: replacement = '{'; break; // ｛
            case 0x70: replacement = '}'; break; // ｝
            case 0x7B: replacement = '+'; break; // ＋
            case 0x7C: replacement = '-'; break; // －
            case 0x81: replacement = '='; break; // ＝
            case 0x83: replacement = '<'; break; // ＜
            case 0x84: replacement = '>'; break; // ＞
            case 0x93: replacement = '%'; break; // ％
            case 0x94: replacement = '#'; break; // ＃
            case 0x95: replacement = '&'; break; // ＆
            case 0x96: replacement = '*'; break; // ＊
            case 0x97: replacement = '@'; break; // ＠
          }
        } else if (ch1 == 0x82) {
          if (ch2 >= 0x4F && ch2 <= 0x58) { // ０-９
            replacement = '0' + (ch2 - 0x4F);
          } else if (ch2 >= 0x60 && ch2 <= 0x79) { // Ａ-Ｚ
            replacement = 'A' + (ch2 - 0x60);
          } else if (ch2 >= 0x81 && ch2 <= 0x9A) { // ａ-ｚ
            replacement = 'a' + (ch2 - 0x81);
          }
        }
        }
        
        if (replacement != 0) {
          s_buffer[outLen++] = replacement;
          modified = true;
          i++; // Skip the second byte
          continue;
        }
        
        // No translation found, copy the full 2-byte char as-is
        s_buffer[outLen++] = lpString[i];
        s_buffer[outLen++] = lpString[i + 1];
        i++;
      } else {
        // Truncated multi-byte
        s_buffer[outLen++] = lpString[i];
      }
      continue;
    }
    
    // Single byte checks
    if (replaceKatakanaDot && (ch1 == 0xA0 || ch1 == '_')) { // Replace NBSP and underscore with ASCII space
      s_buffer[outLen++] = 0x20;  // Replace with ASCII space
      modified = true;
    } else {
      s_buffer[outLen++] = lpString[i];
    }
  }

  if (modified) {
    s_buffer[outLen] = '\0';
    *c_inout = outLen;
    return s_buffer;
  }
  return lpString; // Return original if no changes needed
}

// --- Window Title & Font Size Scaling Hooks ---

static LRESULT CALLBACK MenuWndProc(HWND hWnd, UINT uMsg, WPARAM wParam,
                                    LPARAM lParam);
static const WCHAR *kPrevWndProcProp = L"DC3DDPatchPrevWndProc";

static void ShowVersionInfoDialog(HWND owner) {
  const WCHAR *msg =
      L"D.C. III ~Da Capo III~ Dream Days Ver. 1.00\n"
      L"Data Ver. 1.00";
  MessageBoxW(owner, msg, L"Informasi Versi", MB_OK | MB_ICONINFORMATION);
}

static void RelabelSystemMenu(HMENU hSysMenu) {
  if (!hSysMenu) {
    return;
  }

  int itemCount = GetMenuItemCount(hSysMenu);
  TraceLog("RelabelSystemMenu hMenu=%p itemCount=%d", hSysMenu, itemCount);
  if (itemCount <= 0) {
    return;
  }

  MENUITEMINFOW mii = {};
  mii.cbSize = sizeof(mii);
  mii.fMask = MIIM_FTYPE | MIIM_ID | MIIM_STRING;

  // Find Close item, then rebuild everything after it as exactly:
  // separator + Informasi Versi + separator + DC3DD Patch Settings... (F11)
  int closePos = -1;
  for (int i = 0; i < itemCount; ++i) {
    UINT id = GetMenuItemID(hSysMenu, i);
    if ((id & 0xFFF0) == (SC_CLOSE & 0xFFF0)) {
      closePos = i;
      break;
    }
  }
  if (closePos < 0) {
    return;
  }

  // Discover info/version command id from current tail entries first,
  // before we delete them.
  UINT_PTR infoCmd = 0;
  for (int i = closePos + 1; i < itemCount; ++i) {
    WCHAR txt[256] = {};
    mii.dwTypeData = txt;
    mii.cch = 255;
    if (!GetMenuItemInfoW(hSysMenu, (UINT)i, TRUE, &mii)) continue;
    if (mii.fType & MFT_SEPARATOR) continue;

    if (wcscmp(txt, L"バージョン情報") == 0 ||
        wcscmp(txt, L"Informasi Versi") == 0 ||
        wcscmp(txt, L"ウインドウサイズを元に戻す") == 0 ||
        wcscmp(txt, L"ウィンドウサイズを元に戻す") == 0) {
      infoCmd = mii.wID;
      break;
    }
    if (infoCmd == 0) {
      infoCmd = mii.wID;
    }
  }
  if (infoCmd == 0) infoCmd = g_originalVersionCmd;
  if (infoCmd == 0) infoCmd = g_cmdRestoreWindowSize;
  if (infoCmd == 0) infoCmd = 0x038B;
  g_originalVersionCmd = infoCmd;

  for (int i = itemCount - 1; i > closePos; --i) {
    DeleteMenu(hSysMenu, (UINT)i, MF_BYPOSITION);
  }

  AppendMenuW(hSysMenu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(hSysMenu, MF_STRING, infoCmd, L"Informasi Versi");
  AppendMenuW(hSysMenu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(hSysMenu, MF_STRING, kCmdPatchSettings,
              L"DC3DD Patch Settings... (F11)");

  g_cmdRestoreWindowSize = infoCmd;
  g_cmdVersionInfo = kCmdPatchSettings;
  TraceLog("Relabel rebuilt tail closePos=%d infoCmd=%u patchCmd=%u", closePos,
           (unsigned)infoCmd, (unsigned)kCmdPatchSettings);
}

static void HookMainWindowIfNeeded(HWND hWnd, HWND hWndParent) {
  if (!hWnd || hWndParent != NULL) {
    return;
  }
  TraceLog("HookMainWindowIfNeeded hwnd=%p parent=%p", hWnd, hWndParent);

  if (!g_mainWindow) {
    g_mainWindow = hWnd;
  }
  RelabelSystemMenu(GetSystemMenu(hWnd, FALSE));

  if (!GetPropW(hWnd, kPrevWndProcProp)) {
    WNDPROC prev =
        (WNDPROC)SetWindowLongPtrW(hWnd, GWLP_WNDPROC, (LONG_PTR)MenuWndProc);
    if (prev) {
      SetPropW(hWnd, kPrevWndProcProp, (HANDLE)prev);
    }
  }
}

static decltype(&CreateWindowExW) Real_CreateWindowExW = CreateWindowExW;
static HWND WINAPI Hook_CreateWindowExW(DWORD dwExStyle, LPCWSTR lpClassName, LPCWSTR lpWindowName, DWORD dwStyle, int X, int Y, int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam) {
    if (lpWindowName && wcsstr(lpWindowName, L"D.C.") != NULL) {
        lpWindowName = L"D.C. III ~Da Capo III~ Dream Days";
    }
    HWND hWnd = Real_CreateWindowExW(dwExStyle, lpClassName, lpWindowName, dwStyle, X, Y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam);
    HookMainWindowIfNeeded(hWnd, hWndParent);
    return hWnd;
}

static decltype(&SetWindowTextA) Real_SetWindowTextA = SetWindowTextA;
static BOOL WINAPI Hook_SetWindowTextA(HWND hWnd, LPCSTR lpString) {
    if (lpString && strstr(lpString, "D.C.") != NULL) {
        lpString = "D.C. III ~Da Capo III~ Dream Days";
    }
    return Real_SetWindowTextA(hWnd, lpString);
}

static decltype(&SetWindowTextW) Real_SetWindowTextW = SetWindowTextW;
static BOOL WINAPI Hook_SetWindowTextW(HWND hWnd, LPCWSTR lpString) {
    if (lpString && wcsstr(lpString, L"D.C.") != NULL) {
        lpString = L"D.C. III ~Da Capo III~ Dream Days";
    }
    return Real_SetWindowTextW(hWnd, lpString);
}


// Guard flag: disable font hooks while ChooseFont dialog is open
static bool g_inChooseFont = false;

static decltype(&CreateFontIndirectA) Real_CreateFontIndirectA = CreateFontIndirectA;
static HFONT WINAPI Hook_CreateFontIndirectA(LOGFONTA *lplf) {
    if (lplf && !g_inChooseFont) {
        TraceLog("Hook_CreateFontIndirectA: height=%d", lplf->lfHeight);
        int height = lplf->lfHeight;
        int absHeight = height < 0 ? -height : height;
        if (absHeight >= 12 && absHeight < 200) {
            int desired = g_fontManager.GetDialogueFontSize();
            if (desired != 0) {
                 static LOGFONTA s_lfCacheA[32];
                 static int s_lfCacheIdxA = 0;
                 LOGFONTA* lfCopy = &s_lfCacheA[(s_lfCacheIdxA++) % 32];
                 *lfCopy = *lplf;
                 lfCopy->lfHeight = (height < 0) ? -abs(desired) : abs(desired);
                 lfCopy->lfWidth = 0;
                 std::wstring faceW = g_fontManager.GetDialogueFontName();
                 if (faceW != L"MS Gothic" && faceW != L"ＭＳ ゴシック" && faceW.length() > 0) {
                     WideCharToMultiByte(CP_ACP, 0, faceW.c_str(), -1, lfCopy->lfFaceName, LF_FACESIZE, NULL, NULL);
                 }
                 return Real_CreateFontIndirectA(lfCopy);
            }
        }
    }
    return Real_CreateFontIndirectA(lplf);
}

static decltype(&CreateFontIndirectW) Real_CreateFontIndirectW = CreateFontIndirectW;
static HFONT WINAPI Hook_CreateFontIndirectW(LOGFONTW *lplf) {
    if (lplf && !g_inChooseFont) {
        TraceLog("Hook_CreateFontIndirectW: height=%d, width=%d", lplf->lfHeight, lplf->lfWidth);
        int height = lplf->lfHeight;
        int absHeight = height < 0 ? -height : height;
        if (absHeight >= 12 && absHeight < 200) {
            int desired = g_fontManager.GetDialogueFontSize();
            if (desired != 0) {
                 static LOGFONTW s_lfCacheW[32];
                 static int s_lfCacheIdxW = 0;
                 LOGFONTW* lfCopy = &s_lfCacheW[(s_lfCacheIdxW++) % 32];
                 *lfCopy = *lplf;
                 lfCopy->lfHeight = (height < 0) ? -abs(desired) : abs(desired);
                 lfCopy->lfWidth = 0;
                 std::wstring faceW = g_fontManager.GetDialogueFontName();
                 if (faceW != L"MS Gothic" && faceW != L"ＭＳ ゴシック" && faceW.length() > 0) {
                     wcscpy_s(lfCopy->lfFaceName, LF_FACESIZE, faceW.c_str());
                 }
                 return Real_CreateFontIndirectW(lfCopy);
            }
        }
    }
    return Real_CreateFontIndirectW(lplf);
}

static decltype(&CreateFontA) Real_CreateFontA = CreateFontA;
static HFONT WINAPI Hook_CreateFontA(int cHeight, int cWidth, int cEscapement, int cOrientation, int cWeight, DWORD bItalic, DWORD bUnderline, DWORD bStrikeOut, DWORD iCharSet, DWORD iOutPrecision, DWORD iClipPrecision, DWORD iQuality, DWORD iPitchAndFamily, LPCSTR pszFaceName) {
    if (!g_inChooseFont) {
        TraceLog("Hook_CreateFontA: height=%d", cHeight);
      int absHeight = cHeight < 0 ? -cHeight : cHeight;
      if (absHeight >= 12 && absHeight < 200) {
          int desired = g_fontManager.GetDialogueFontSize();
          if (desired != 0) {
               cHeight = (cHeight < 0) ? -abs(desired) : abs(desired);
               cWidth = 0;
          }
      }
    }
    return Real_CreateFontA(cHeight, cWidth, cEscapement, cOrientation, cWeight, bItalic, bUnderline, bStrikeOut, iCharSet, iOutPrecision, iClipPrecision, iQuality, iPitchAndFamily, pszFaceName);
}

static HFONT WINAPI Hook_CreateFontW(int cHeight, int cWidth, int cEscapement, int cOrientation, int cWeight, DWORD bItalic, DWORD bUnderline, DWORD bStrikeOut, DWORD iCharSet, DWORD iOutPrecision, DWORD iClipPrecision, DWORD iQuality, DWORD iPitchAndFamily, LPCWSTR pszFaceName) {
    if (!g_inChooseFont) {
        TraceLog("Hook_CreateFontW: height=%d, width=%d", cHeight, cWidth);
      int absHeight = cHeight < 0 ? -cHeight : cHeight;
      if (absHeight >= 12 && absHeight < 200) {
          int desired = g_fontManager.GetDialogueFontSize();
          if (desired != 0) {
               cHeight = (cHeight < 0) ? -abs(desired) : abs(desired);
               cWidth = 0;
          }
      }
    }

    return Real_CreateFontW(cHeight, cWidth, cEscapement, cOrientation, cWeight, bItalic, bUnderline, bStrikeOut, iCharSet, iOutPrecision, iClipPrecision, iQuality, iPitchAndFamily, pszFaceName);
}

static bool IsUiElement(LPCSTR str, UINT c) {
  if (!str || c == 0)
    return false;


  // Ensure null termination for safe strstr scanning if it's not already null-terminated
  static thread_local char s_ui_buffer[1024];
  UINT len = (c >= sizeof(s_ui_buffer)) ? sizeof(s_ui_buffer) - 1 : c;
  memcpy(s_ui_buffer, str, len);
  s_ui_buffer[len] = '\0';

  const char *ui_elements[] = {"Scene Jump",
                               "Putar Suara",
                               "Kembali",
                               "Aktifkan Thumbnail",
                               "シーンジャンプ",
                               "音声再生",
                               "戻る",
                               "サムネイル有効",
                               "LOG",
                               "Log",
                               "log"};
  for (const auto &elem : ui_elements) {
    if (strstr(s_ui_buffer, elem) != nullptr) {
      return true;
    }
  }
  return false;
}

// Hook ExtTextOutA - UI Text Rendering (e.g. Character Names in Backlog)

static int SafeStrLenA(LPCSTR str, int maxLen = 1000) {
    if (!str || IsBadReadPtr(str, 1)) return 0;
    int len = 0;
    while (len < maxLen) {
        if (IsBadReadPtr(str + len, 1)) break;
        if (str[len] == '\0') break;
        len++;
    }
    return len;
}

static int SafeStrLenW(LPCWSTR str, int maxLen = 1000) {
    if (!str || IsBadReadPtr(str, sizeof(WCHAR))) return 0;
    int len = 0;
    while (len < maxLen) {
        if (IsBadReadPtr(str + len, sizeof(WCHAR))) break;
        if (str[len] == L'\0') break;
        len++;
    }
    return len;
}

static BOOL WINAPI Hook_ExtTextOutA(HDC hdc, int x, int y, UINT options,
                                    const RECT *lprect, LPCSTR lpString, UINT c,
                                    const INT *lpDx) {

  if (lpString == NULL || (options & ETO_GLYPH_INDEX) || IsBadReadPtr(lpString, 1)) {
      return Real_ExtTextOutA(hdc, x, y, options, lprect, lpString, c, lpDx);
  }
  UINT originalC = c;
  int actualLen = SafeStrLenA(lpString);
  // If c==0, treat as null-terminated (so _ -> space filter still applies)
  if (c == 0 && lpString && lpString[0] != 0) {
    c = (UINT)actualLen;
  } else if (c > 0 && actualLen > c && actualLen < c + 150) {
    c = actualLen;
  }

  UINT renderLen = c;
  bool performTranslation = !(g_inBacklogRender && g_fontManager.GetDisableBacklogTranslation());
  LPCSTR renderStr = FilterSjisString(lpString, &renderLen, performTranslation, true);

  // Check for scenario title translation (save/load screen)
  static thread_local char s_scenarioTitleBuf[256];
  {
    // Build null-terminated copy for lookup
    UINT lookupLen = (renderLen < sizeof(s_scenarioTitleBuf) - 1) ? renderLen : sizeof(s_scenarioTitleBuf) - 1;
    memcpy(s_scenarioTitleBuf, renderStr, lookupLen);
    s_scenarioTitleBuf[lookupLen] = '\0';



    const char* translated = g_fontManager.TranslateScenarioTitle(s_scenarioTitleBuf);
    if (translated) {
      renderStr = translated;
      renderLen = (UINT)strlen(translated);
    }
  }

  static thread_local HDC s_lastHdc_ExtTextOut = NULL;
  static thread_local HFONT s_lastBgFont_ExtTextOut = NULL;
  static thread_local int s_lastTmHeight_ExtTextOut = 0;
  
  HFONT currFont = (HFONT)GetCurrentObject(hdc, OBJ_FONT);
  if (hdc != s_lastHdc_ExtTextOut || currFont != s_lastBgFont_ExtTextOut || s_lastHdc_ExtTextOut == NULL) {
    TEXTMETRICA tm;
    Real_GetTextMetricsA(hdc, &tm);
    s_lastTmHeight_ExtTextOut = tm.tmHeight;
    s_lastHdc_ExtTextOut = hdc;
    s_lastBgFont_ExtTextOut = currFont;
  }

  bool isName = false;
  if (g_inBacklogRender && !IsUiElement(lpString, c)) {
    isName = (s_lastTmHeight_ExtTextOut <= 18);
  }

  int nameSpacing = isName ? g_fontManager.GetBacklogNameSpacing() : 0;
  
  if (g_inBacklogRender && g_fontManager.GetDisableBacklogSpacing()) {
    nameSpacing = 0; // Skip character spacing
  }

  HFONT customFont = NULL;
  HFONT oldFont = NULL;

  bool hasCustomFont = false;
  if (isName) {
    hasCustomFont = ((g_fontManager.GetBacklogNameFontSize() != 0) ||
                     (g_fontManager.GetBacklogNameFontName() != L"MS Gothic" && 
                      g_fontManager.GetBacklogNameFontName() != L"ＭＳ ゴシック") ||
                     (nameSpacing != 0));
  } else if (g_inBacklogRender) {
    hasCustomFont = ((g_fontManager.GetBacklogFontSize() != 0) ||
                     (g_fontManager.GetBacklogFontName() != L"MS Gothic" && 
                      g_fontManager.GetBacklogFontName() != L"ＭＳ ゴシック"));
  }

  if (g_inBacklogRender && g_fontManager.GetDisableBacklogFont()) {
    hasCustomFont = false;
  }

  if (hasCustomFont) {
    if (isName) {
      customFont = g_fontManager.GetBacklogNameFont(s_lastTmHeight_ExtTextOut);
    } else {
      customFont = g_fontManager.GetBacklogFont(s_lastTmHeight_ExtTextOut);
    }
    oldFont = (HFONT)SelectObject(hdc, customFont);
  }

  int renderX = x;
  int renderY = y;
  if (!g_fontManager.GetDisableBacklogSpacing()) {
    if (g_inBacklogRender) {
      if (hasCustomFont && isName) {
        renderY += g_fontManager.GetBacklogNameYOffset();
      }
    } else {
      if (!IsUiElement(lpString, c)) {
        renderY += g_fontManager.GetDialogueYOffset();
      }
    }
  }

  if (nameSpacing != 0) {
    SetTextCharacterExtra(hdc, nameSpacing);
  }

  if (g_inBacklogRender && !isName) {
    // Enable opaque background for Dialog Text to fill the spaced gaps
    SetBkMode(hdc, OPAQUE);
    // Dark transparent-like blue matching the BKLOGCHIP theme (RGB(0, 140,
    // 255))
    SetBkColor(hdc, RGB(0, 140, 255));
  } else {
    // Names and normal text remain transparent
    SetBkMode(hdc, TRANSPARENT);
  }

  RECT fixedRect = {0};
  const RECT* finalRect = lprect;
  if (lprect != NULL && (renderX != x || renderY != y)) {
      fixedRect = *lprect;
      int deltaX = renderX - x;
      int deltaY = renderY - y;
      fixedRect.left += deltaX;
      fixedRect.right += deltaX;
      fixedRect.top += deltaY;
      fixedRect.bottom += deltaY;
      finalRect = &fixedRect;
  }
  BOOL result = Real_ExtTextOutA(hdc, renderX, renderY, options, finalRect, renderStr, renderLen, lpDx);

  SetBkMode(hdc, TRANSPARENT);

  if (nameSpacing != 0) {
    SetTextCharacterExtra(hdc, 0); // Restore
  }

  if (oldFont) {
    SelectObject(hdc, oldFont);
  }

  return result;
}

static decltype(&ExtTextOutW) Real_ExtTextOutW = ExtTextOutW;
static BOOL WINAPI Hook_ExtTextOutW(HDC hdc, int x, int y, UINT options,
                                    const RECT *lprect, LPCWSTR lpString, UINT c,
                                    const INT *lpDx) {
  int renderX = x;
  int renderY = y;
  if (!g_fontManager.GetDisableBacklogSpacing() && !g_inBacklogRender) {
    renderX += g_fontManager.GetDialogueXOffset();
    renderY += g_fontManager.GetDialogueYOffset();
  }

  RECT fixedRect = {0};
  const RECT* finalRect = lprect;
  if (lprect != NULL && (renderX != x || renderY != y)) {
      fixedRect = *lprect;
      int deltaX = renderX - x;
      int deltaY = renderY - y;
      fixedRect.left += deltaX;
      fixedRect.right += deltaX;
      fixedRect.top += deltaY;
      fixedRect.bottom += deltaY;
      finalRect = &fixedRect;
  }

  if (lpString != NULL && c > 0 && c < 10000 && !(options & ETO_GLYPH_INDEX) && !IsBadReadPtr(lpString, c * sizeof(WCHAR))) {
    __try {
        WCHAR* buffer = new WCHAR[c + 1];
        for (UINT i = 0; i < c; i++) {
            if (lpString[i] == L'_') {
                buffer[i] = L'X';
            } else {
                buffer[i] = lpString[i];
            }
        }
        buffer[c] = L'\0';
        BOOL result = Real_ExtTextOutW(hdc, renderX, renderY, options, finalRect, buffer, c, lpDx);
        delete[] buffer;
        return result;
    } __except(1) {
        // Access violation or similar reading lpString
    }
  }
  return Real_ExtTextOutW(hdc, renderX, renderY, options, finalRect, lpString, c, lpDx);
}

static decltype(&TextOutA) Real_TextOutA = TextOutA;
static BOOL WINAPI Hook_TextOutA(HDC hdc, int x, int y, LPCSTR lpString, int c) {
  if (lpString == NULL || IsBadReadPtr(lpString, 1)) return Real_TextOutA(hdc, x, y, lpString, c);
  if (lpString == NULL || IsBadReadPtr(lpString, 1)) return Real_TextOutA(hdc, x, y, lpString, c);
  if (lpString == NULL || IsBadReadPtr(lpString, 1)) return Real_TextOutA(hdc, x, y, lpString, c);
  if (lpString == NULL || IsBadReadPtr(lpString, 1)) return Real_TextOutA(hdc, x, y, lpString, c);
  if (lpString == NULL || IsBadReadPtr(lpString, 1)) return Real_TextOutA(hdc, x, y, lpString, c);
  int actualLen = SafeStrLenA(lpString);
  if (c == 0 && lpString && lpString[0] != 0) {
    c = actualLen;
  } else if (c > 0 && actualLen > c && actualLen < c + 150) {
    c = actualLen;
  }

  UINT renderLen = c;
  bool performTranslation = !(g_inBacklogRender && g_fontManager.GetDisableBacklogTranslation());
  LPCSTR renderStr = FilterSjisString(lpString, &renderLen, performTranslation, true);

  int renderX = x;
  int renderY = y;
  if (!g_inBacklogRender && !IsUiElement(renderStr, renderLen)) {
      renderX += g_fontManager.GetDialogueXOffset();
      renderY += g_fontManager.GetDialogueYOffset();
  }

  return Real_TextOutA(hdc, renderX, renderY, renderStr, renderLen);
}

static decltype(&TextOutW) Real_TextOutW = TextOutW;
static BOOL WINAPI Hook_TextOutW(HDC hdc, int x, int y, LPCWSTR lpString, int c) {
  int renderX = x;
  int renderY = y;
  if (!g_inBacklogRender) {
      renderX += g_fontManager.GetDialogueXOffset();
      renderY += g_fontManager.GetDialogueYOffset();
  }
  return Real_TextOutW(hdc, renderX, renderY, lpString, c);
}

static decltype(&DrawTextA) Real_DrawTextA = DrawTextA;
static int WINAPI Hook_DrawTextA(HDC hdc, LPCSTR lpchText, int cchText, LPRECT lprc, UINT format) {
  if (lpchText == NULL || IsBadReadPtr(lpchText, 1)) return Real_DrawTextA(hdc, lpchText, cchText, lprc, format);
  int len = cchText;
  if (len == -1) len = SafeStrLenA(lpchText);
  UINT renderLen = len;
  bool performTranslation = !(g_inBacklogRender && g_fontManager.GetDisableBacklogTranslation());
  LPCSTR renderStr = FilterSjisString(lpchText, &renderLen, performTranslation, true);

  RECT fixedRect = {0};
  const RECT* finalRect = lprc;
  if (!g_inBacklogRender && !IsUiElement(renderStr, renderLen)) {
    int xOffset = g_fontManager.GetDialogueXOffset();
    int yOffset = g_fontManager.GetDialogueYOffset();
    if (lprc != NULL && (xOffset != 0 || yOffset != 0)) {
      fixedRect = *lprc;
      fixedRect.left += xOffset;
      fixedRect.right += xOffset;
      fixedRect.top += yOffset;
      fixedRect.bottom += yOffset;
      finalRect = &fixedRect;
    }
  }

  int result = Real_DrawTextA(hdc, renderStr, renderLen, (LPRECT)finalRect, format);
  if (lprc != NULL && finalRect == &fixedRect && (format & DT_CALCRECT)) {
      lprc->left = fixedRect.left - g_fontManager.GetDialogueXOffset();
      lprc->right = fixedRect.right - g_fontManager.GetDialogueXOffset();
      lprc->top = fixedRect.top - g_fontManager.GetDialogueYOffset();
      lprc->bottom = fixedRect.bottom - g_fontManager.GetDialogueYOffset();
  }
  return result;
}

static decltype(&DrawTextW) Real_DrawTextW = DrawTextW;
static int WINAPI Hook_DrawTextW(HDC hdc, LPCWSTR lpchText, int cchText, LPRECT lprc, UINT format) {
  if (lpchText == NULL || IsBadReadPtr(lpchText, sizeof(WCHAR))) return Real_DrawTextW(hdc, lpchText, cchText, lprc, format);
  int len = cchText;
  if (len == -1) {
      len = 0;
      while (lpchText[len] != L'\0' && len < 10000) len++;
  }

  RECT fixedRect = {0};
  const RECT* finalRect = lprc;
  if (!g_inBacklogRender) {
    int xOffset = g_fontManager.GetDialogueXOffset();
    int yOffset = g_fontManager.GetDialogueYOffset();
    if (lprc != NULL && (xOffset != 0 || yOffset != 0)) {
      fixedRect = *lprc;
      fixedRect.left += xOffset;
      fixedRect.right += xOffset;
      fixedRect.top += yOffset;
      fixedRect.bottom += yOffset;
      finalRect = &fixedRect;
    }
  }

  WCHAR* buffer = new WCHAR[len + 1];
  for (int i = 0; i < len; i++) {
      if (lpchText[i] == L'_' || lpchText[i] == 0xFF3F) buffer[i] = L'X';
      else buffer[i] = lpchText[i];
  }
  buffer[len] = L'\0';
  int result = Real_DrawTextW(hdc, buffer, len, (LPRECT)finalRect, format);
  if (lprc != NULL && finalRect == &fixedRect && (format & DT_CALCRECT)) {
      lprc->left = fixedRect.left - g_fontManager.GetDialogueXOffset();
      lprc->right = fixedRect.right - g_fontManager.GetDialogueXOffset();
      lprc->top = fixedRect.top - g_fontManager.GetDialogueYOffset();
      lprc->bottom = fixedRect.bottom - g_fontManager.GetDialogueYOffset();
  }
  delete[] buffer;
  return result;
}

static decltype(&DrawTextExA) Real_DrawTextExA = DrawTextExA;
static int WINAPI Hook_DrawTextExA(HDC hdc, LPSTR lpchText, int cchText, LPRECT lprc, UINT format, LPDRAWTEXTPARAMS lpdtp) {
  if (lpchText == NULL || IsBadReadPtr(lpchText, 1)) return Real_DrawTextExA(hdc, lpchText, cchText, lprc, format, lpdtp);
  int len = cchText;
  if (len == -1) len = SafeStrLenA(lpchText);
  UINT renderLen = len;
  bool performTranslation = !(g_inBacklogRender && g_fontManager.GetDisableBacklogTranslation());
  LPCSTR renderStr = FilterSjisString(lpchText, &renderLen, performTranslation, true);

  RECT fixedRect = {0};
  const RECT* finalRect = lprc;
  if (!g_inBacklogRender && !IsUiElement(renderStr, renderLen)) {
    int xOffset = g_fontManager.GetDialogueXOffset();
    int yOffset = g_fontManager.GetDialogueYOffset();
    if (lprc != NULL && (xOffset != 0 || yOffset != 0)) {
      fixedRect = *lprc;
      fixedRect.left += xOffset;
      fixedRect.right += xOffset;
      fixedRect.top += yOffset;
      fixedRect.bottom += yOffset;
      finalRect = &fixedRect;
    }
  }

  int result = Real_DrawTextExA(hdc, (LPSTR)renderStr, renderLen, (LPRECT)finalRect, format, lpdtp);
  if (lprc != NULL && finalRect == &fixedRect && (format & DT_CALCRECT)) {
      lprc->left = fixedRect.left - g_fontManager.GetDialogueXOffset();
      lprc->right = fixedRect.right - g_fontManager.GetDialogueXOffset();
      lprc->top = fixedRect.top - g_fontManager.GetDialogueYOffset();
      lprc->bottom = fixedRect.bottom - g_fontManager.GetDialogueYOffset();
  }
  return result;
}

static decltype(&DrawTextExW) Real_DrawTextExW = DrawTextExW;
static int WINAPI Hook_DrawTextExW(HDC hdc, LPWSTR lpchText, int cchText, LPRECT lprc, UINT format, LPDRAWTEXTPARAMS lpdtp) {
  if (lpchText == NULL || IsBadReadPtr(lpchText, sizeof(WCHAR))) return Real_DrawTextExW(hdc, lpchText, cchText, lprc, format, lpdtp);
  int len = cchText;
  if (len == -1) {
      len = 0;
      while (lpchText[len] != L'\0' && len < 10000) len++;
  }

  RECT fixedRect = {0};
  const RECT* finalRect = lprc;
  if (!g_inBacklogRender) {
    int xOffset = g_fontManager.GetDialogueXOffset();
    int yOffset = g_fontManager.GetDialogueYOffset();
    if (lprc != NULL && (xOffset != 0 || yOffset != 0)) {
      fixedRect = *lprc;
      fixedRect.left += xOffset;
      fixedRect.right += xOffset;
      fixedRect.top += yOffset;
      fixedRect.bottom += yOffset;
      finalRect = &fixedRect;
    }
  }

  WCHAR* buffer = new WCHAR[len + 1];
  for (int i = 0; i < len; i++) {
      if (lpchText[i] == L'_' || lpchText[i] == 0xFF3F) buffer[i] = L'X';
      else buffer[i] = lpchText[i];
  }
  buffer[len] = L'\0';
  int result = Real_DrawTextExW(hdc, buffer, len, (LPRECT)finalRect, format, lpdtp);
  if (lprc != NULL && finalRect == &fixedRect && (format & DT_CALCRECT)) {
      lprc->left = fixedRect.left - g_fontManager.GetDialogueXOffset();
      lprc->right = fixedRect.right - g_fontManager.GetDialogueXOffset();
      lprc->top = fixedRect.top - g_fontManager.GetDialogueYOffset();
      lprc->bottom = fixedRect.bottom - g_fontManager.GetDialogueYOffset();
  }
  delete[] buffer;
  return result;
}

static BOOL WINAPI Hook_GetTextExtentPoint32A(
HDC hdc, LPCSTR lpString, int c,
                                              LPSIZE lpSize) {
  if (lpString == NULL || IsBadReadPtr(lpString, 1)) return Real_GetTextExtentPoint32A(hdc, lpString, c, lpSize);
  if (lpString == NULL || IsBadReadPtr(lpString, 1)) return Real_GetTextExtentPoint32A(hdc, lpString, c, lpSize);
  if (lpString == NULL || IsBadReadPtr(lpString, 1)) return Real_GetTextExtentPoint32A(hdc, lpString, c, lpSize);
  int actualLen = SafeStrLenA(lpString);
  if (c > 0 && actualLen > c && actualLen < c + 150)
    c = actualLen;
  
  UINT filteredLen = c;
  bool performTranslation = !(g_inBacklogRender && g_fontManager.GetDisableBacklogTranslation());
  lpString = FilterSjisString(lpString, &filteredLen, performTranslation, false);
  c = filteredLen;

  static thread_local HDC s_lastHdc_GetTextExtent = NULL;
  static thread_local HFONT s_lastBgFont_GetTextExtent = NULL;
  static thread_local int s_lastTmHeight_GetTextExtent = 0;
  
  HFONT currFont = (HFONT)GetCurrentObject(hdc, OBJ_FONT);
  if (hdc != s_lastHdc_GetTextExtent || currFont != s_lastBgFont_GetTextExtent || s_lastHdc_GetTextExtent == NULL) {
    TEXTMETRICA tm;
    Real_GetTextMetricsA(hdc, &tm);
    s_lastTmHeight_GetTextExtent = tm.tmHeight;
    s_lastHdc_GetTextExtent = hdc;
    s_lastBgFont_GetTextExtent = currFont;
  }

  bool isName = false;
  if (g_inBacklogRender && !IsUiElement(lpString, c)) {
    isName = (s_lastTmHeight_GetTextExtent <= 18);
  }

  int nameSpacing = isName ? g_fontManager.GetBacklogNameSpacing() : 0;
  if (g_inBacklogRender && g_fontManager.GetDisableBacklogSpacing()) {
    nameSpacing = 0;
  }

  HFONT customFont = NULL;
  HFONT oldFont = NULL;

  bool hasCustomFont = false;
  if (isName) {
    hasCustomFont = ((g_fontManager.GetBacklogNameFontSize() != 0) ||
                     (g_fontManager.GetBacklogNameFontName() != L"MS Gothic" && 
                      g_fontManager.GetBacklogNameFontName() != L"ＭＳ ゴシック") ||
                     (nameSpacing != 0));
  } else if (g_inBacklogRender) {
    hasCustomFont = ((g_fontManager.GetBacklogFontSize() != 0) ||
                     (g_fontManager.GetBacklogFontName() != L"MS Gothic" && 
                      g_fontManager.GetBacklogFontName() != L"ＭＳ ゴシック"));
  }

  if (g_inBacklogRender && g_fontManager.GetDisableBacklogFont()) {
    hasCustomFont = false;
  }

  if (hasCustomFont) {
    if (isName) {
      customFont = g_fontManager.GetBacklogNameFont(s_lastTmHeight_GetTextExtent);
    } else {
      customFont = g_fontManager.GetBacklogFont(s_lastTmHeight_GetTextExtent);
    }
    oldFont = (HFONT)SelectObject(hdc, customFont);
  }

  if (nameSpacing != 0) {
    SetTextCharacterExtra(hdc, nameSpacing);
  }

  BOOL result = Real_GetTextExtentPoint32A(hdc, lpString, c, lpSize);

  if (!g_fontManager.GetDisableBacklogSpacing()) {
    if (result && g_inBacklogRender && !isName) {
      lpSize->cy += g_fontManager.GetBacklogLineSpacing();
    } else if (result && !g_inBacklogRender && !IsUiElement(lpString, c)) {
      lpSize->cy += g_fontManager.GetDialogueLineSpacing();
    }
  }

  if (nameSpacing != 0) {
    SetTextCharacterExtra(hdc, 0);
  }
  if (oldFont) {
    SelectObject(hdc, oldFont);
  }

  return result;
}

static BOOL WINAPI Hook_GetTextExtentPoint32W(HDC hdc, LPCWSTR lpString, int c,
                                              LPSIZE lpSize) {
  BOOL result = Real_GetTextExtentPoint32W(hdc, lpString, c, lpSize);
  return result;
}

// GetTextExtentExPoint Hooks
static decltype(&GetTextExtentExPointA) Real_GetTextExtentExPointA =
    GetTextExtentExPointA;
static decltype(&GetTextExtentExPointW) Real_GetTextExtentExPointW =
    GetTextExtentExPointW;

static BOOL WINAPI Hook_GetTextExtentExPointA(HDC hdc, LPCSTR lpszString,
                                              int cchString, int nMaxExtent,
                                              LPINT lpnFit, LPINT lpnDx,
                                              LPSIZE lpSize) {
  int actualLen = lstrlenA(lpszString);
  if (cchString > 0 && actualLen > cchString && actualLen < cchString + 150)
    cchString = actualLen;
  
  UINT filteredLen = cchString;
  bool performTranslation = !(g_inBacklogRender && g_fontManager.GetDisableBacklogTranslation());
  lpszString = FilterSjisString(lpszString, &filteredLen, performTranslation, false);
  cchString = filteredLen;

  static thread_local HDC s_lastHdc_GetTextExtentEx = NULL;
  static thread_local HFONT s_lastBgFont_GetTextExtentEx = NULL;
  static thread_local int s_lastTmHeight_GetTextExtentEx = 0;
  
  HFONT currFont = (HFONT)GetCurrentObject(hdc, OBJ_FONT);
  if (hdc != s_lastHdc_GetTextExtentEx || currFont != s_lastBgFont_GetTextExtentEx || s_lastHdc_GetTextExtentEx == NULL) {
    TEXTMETRICA tm;
    Real_GetTextMetricsA(hdc, &tm);
    s_lastTmHeight_GetTextExtentEx = tm.tmHeight;
    s_lastHdc_GetTextExtentEx = hdc;
    s_lastBgFont_GetTextExtentEx = currFont;
  }

  bool isName = false;
  if (g_inBacklogRender && !IsUiElement(lpszString, cchString)) {
    isName = (s_lastTmHeight_GetTextExtentEx <= 18);
  }

  int nameSpacing = isName ? g_fontManager.GetBacklogNameSpacing() : 0;
  if (g_inBacklogRender && g_fontManager.GetDisableBacklogSpacing()) {
    nameSpacing = 0;
  }

  HFONT customFont = NULL;
  HFONT oldFont = NULL;

  bool hasCustomFont = false;
  if (isName) {
    hasCustomFont = ((g_fontManager.GetBacklogNameFontSize() != 0) ||
                     (g_fontManager.GetBacklogNameFontName() != L"MS Gothic" && 
                      g_fontManager.GetBacklogNameFontName() != L"ＭＳ ゴシック") ||
                     (nameSpacing != 0));
  } else if (g_inBacklogRender) {
    hasCustomFont = ((g_fontManager.GetBacklogFontSize() != 0) ||
                     (g_fontManager.GetBacklogFontName() != L"MS Gothic" && 
                      g_fontManager.GetBacklogFontName() != L"ＭＳ ゴシック"));
  }

  if (g_inBacklogRender && g_fontManager.GetDisableBacklogFont()) {
    hasCustomFont = false;
  }

  if (hasCustomFont) {
    if (isName) {
      customFont = g_fontManager.GetBacklogNameFont(s_lastTmHeight_GetTextExtentEx);
    } else {
      customFont = g_fontManager.GetBacklogFont(s_lastTmHeight_GetTextExtentEx);
    }
    oldFont = (HFONT)SelectObject(hdc, customFont);
  }

  if (nameSpacing != 0) {
    SetTextCharacterExtra(hdc, nameSpacing);
  }

  BOOL result = Real_GetTextExtentExPointA(hdc, lpszString, cchString,
                                           nMaxExtent, lpnFit, lpnDx, lpSize);

  if (!g_fontManager.GetDisableBacklogSpacing()) {
    if (result && g_inBacklogRender && !isName && lpSize != NULL) {
      lpSize->cy += g_fontManager.GetBacklogLineSpacing();
    } else if (result && !g_inBacklogRender && !IsUiElement(lpszString, cchString) && lpSize != NULL) {
      lpSize->cy += g_fontManager.GetDialogueLineSpacing();
    }
  }

  if (nameSpacing != 0) {
    SetTextCharacterExtra(hdc, 0);
  }
  if (oldFont) {
    SelectObject(hdc, oldFont);
  }
  return result;
}

static BOOL WINAPI Hook_GetTextExtentExPointW(HDC hdc, LPCWSTR lpszString,
                                              int cchString, int nMaxExtent,
                                              LPINT lpnFit, LPINT lpnDx,
                                              LPSIZE lpSize) {
  BOOL result = Real_GetTextExtentExPointW(hdc, lpszString, cchString,
                                           nMaxExtent, lpnFit, lpnDx, lpSize);
  return result;
}

// GetCharWidth32 Hooks
static decltype(&GetCharWidth32A) Real_GetCharWidth32A = GetCharWidth32A;
static decltype(&GetCharWidth32W) Real_GetCharWidth32W = GetCharWidth32W;

static BOOL WINAPI Hook_GetCharWidth32A(HDC hdc, UINT iFirst, UINT iLast,
                                        LPINT lpBuffer) {
  BOOL res = Real_GetCharWidth32A(hdc, iFirst, iLast, lpBuffer);
  if (res && lpBuffer) {
      if (0x8140 >= iFirst && 0x8140 <= iLast) {
          int spaceWidth = 0;
          if (Real_GetCharWidth32A(hdc, ' ', ' ', &spaceWidth)) {
              lpBuffer[0x8140 - iFirst] = spaceWidth;
          }
      }
  }
  return res;
}

static BOOL WINAPI Hook_GetCharWidth32W(HDC hdc, UINT iFirst, UINT iLast,
                                        LPINT lpBuffer) {
  BOOL res = Real_GetCharWidth32W(hdc, iFirst, iLast, lpBuffer);
  if (res && lpBuffer) {
      if (0x3000 >= iFirst && 0x3000 <= iLast) {
          int spaceWidth = 0;
          if (Real_GetCharWidth32W(hdc, L' ', L' ', &spaceWidth)) {
              lpBuffer[0x3000 - iFirst] = spaceWidth;
          }
      }
  }
  return res;
}


static decltype(&GetGlyphOutlineW) Real_GetGlyphOutlineW = GetGlyphOutlineW;
static DWORD WINAPI Hook_GetGlyphOutlineW(HDC hdc, UINT uChar, UINT fuFormat,
                                          LPGLYPHMETRICS lpgm, DWORD cjBuffer,
                                          LPVOID pvBuffer, const MAT2 *lpmat2) {
  if (uChar == 0x3000) uChar = L' ';
  else if (uChar == 0xA0 || uChar == L'_' || uChar == 0xFF3F) uChar = L' ';
  DWORD r = Real_GetGlyphOutlineW(hdc, uChar, fuFormat, lpgm, cjBuffer, pvBuffer, lpmat2);
  
  if (r != GDI_ERROR && lpgm && !g_inBacklogRender && !g_fontManager.GetDisableBacklogSpacing()) {
      lpgm->gmptGlyphOrigin.x += g_fontManager.GetDialogueXOffset();
      lpgm->gmptGlyphOrigin.y -= g_fontManager.GetDialogueYOffset(); // Larger origin Y = drawn higher
  }
  return r;
}
static DWORD WINAPI Hook_GetGlyphOutlineA(HDC hdc, UINT uChar, UINT fuFormat,
                                          LPGLYPHMETRICS lpgm, DWORD cjBuffer,
                                          LPVOID pvBuffer, const MAT2 *lpmat2) {
  // DC3WY approach: detect underscore (space placeholder), render normally
  // to get correct single-byte advance width, then zero out glyph bitmap
  bool isUnderscore = (uChar == '_');

  DWORD r = GDI_ERROR;

  static thread_local HDC s_lastHdc_GlyphOutline = NULL;
  static thread_local HFONT s_lastBgFont_GlyphOutline = NULL;
  static thread_local int s_lastTmHeight_GlyphOutline = 0;

  HFONT currFont = (HFONT)GetCurrentObject(hdc, OBJ_FONT);
  if (hdc != s_lastHdc_GlyphOutline || currFont != s_lastBgFont_GlyphOutline || s_lastHdc_GlyphOutline == NULL) {
    TEXTMETRICA tm;
    if (Real_GetTextMetricsA(hdc, &tm)) {
      s_lastTmHeight_GlyphOutline = tm.tmHeight;
    } else {
      s_lastTmHeight_GlyphOutline = 0; // Fallback
    }
    s_lastHdc_GlyphOutline = hdc;
    s_lastBgFont_GlyphOutline = currFont;
  }

  bool isName = false;
  
  if (g_inBacklogRender && s_lastTmHeight_GlyphOutline > 0 && !g_fontManager.GetDisableBacklogFont()) {
    int fontSize = ScaleBacklogFontSize(s_lastTmHeight_GlyphOutline);
    isName = (s_lastTmHeight_GlyphOutline <= 18);
    
    HFONT f = NULL;
    if (isName) {
      f = g_fontManager.GetBacklogNameFont(fontSize);
    } else {
      f = g_fontManager.GetBacklogFont(fontSize);
    }

    if (f) {
      SelectObject(hdc, f); // Permanently mutate HDC for this loop
      r = Real_GetGlyphOutlineA(hdc, uChar, fuFormat, lpgm, cjBuffer, pvBuffer,
                                lpmat2);
    }
  }

  if (r == GDI_ERROR) {
    r = Real_GetGlyphOutlineA(hdc, uChar, fuFormat, lpgm, cjBuffer, pvBuffer,
                              lpmat2);
  }

  // Zero out the glyph bitmap for underscore so it renders as invisible space
  // while preserving gmCellIncX (the original single-byte advance width)
  if (r != GDI_ERROR && isUnderscore) {
    if (pvBuffer != NULL && r > 0) {
      memset(pvBuffer, 0, r);
    }
    // gmCellIncX retains its original underscore width naturally
  }

  if (r != GDI_ERROR && lpgm && !g_fontManager.GetDisableBacklogSpacing()) {
    if (g_inBacklogRender) {
      if (!isName) {
        lpgm->gmptGlyphOrigin.x += g_fontManager.GetBacklogXOffset();
        lpgm->gmptGlyphOrigin.y += g_fontManager.GetBacklogYOffset();
        lpgm->gmCellIncX += g_fontManager.GetBacklogDialogSpacing();
      } else {
        lpgm->gmptGlyphOrigin.x += g_fontManager.GetBacklogNameXOffset();
      }
    } else {
      lpgm->gmptGlyphOrigin.x += g_fontManager.GetDialogueXOffset();
      lpgm->gmptGlyphOrigin.y -= g_fontManager.GetDialogueYOffset(); // Larger origin Y = drawn higher
    }
  }

  return r;
}

static HANDLE WINAPI Hook_CreateFileA(
    LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition,
    DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) {

  static thread_local bool s_inCreateFile = false;
  if (s_inCreateFile) {
    return Real_CreateFileA(lpFileName, dwDesiredAccess, dwShareMode,
                            lpSecurityAttributes, dwCreationDisposition,
                            dwFlagsAndAttributes, hTemplateFile);
  }

  s_inCreateFile = true;

  int lang = g_fontManager.GetLanguage();
  if (lang == 1 || lang == 2) {
    std::string newPath = ReplacePathA(lpFileName);
    if (!newPath.empty()) {
      // Resolve to absolute path to avoid CWD-dependent resolution issues
      char absPath[MAX_PATH];
      DWORD absLen = GetFullPathNameA(newPath.c_str(), MAX_PATH, absPath, NULL);
      const char* finalPath = (absLen > 0 && absLen < MAX_PATH) ? absPath : newPath.c_str();

      HANDLE h = Real_CreateFileA(finalPath, dwDesiredAccess, dwShareMode,
                                  lpSecurityAttributes, dwCreationDisposition,
                                  dwFlagsAndAttributes, hTemplateFile);
      s_inCreateFile = false;
      return h;
    }
  }

  HANDLE h = Real_CreateFileA(lpFileName, dwDesiredAccess, dwShareMode,
                              lpSecurityAttributes, dwCreationDisposition,
                              dwFlagsAndAttributes, hTemplateFile);
  s_inCreateFile = false;
  return h;
}

// ============================================================================
// DVD Verification Bypass - Direct Memory Patch
// ============================================================================
// From x32dbg disassembly of the DVD check function:
//
//   004100C7: call 40FFE0          ; FindDvdDrive()
//   004100CF: test al, al          ; check result
//   004100D1: je 0x4100F8          ; ← if DVD NOT found, jump to error path
//   ...                            ;   (stores drive letter, etc.)
//   004100F2: mov eax, 1           ; return 1 (success)
//   004100F7: ret
//   004100F8: push esi             ; ← error path: show MessageBox + retry
//   loop
//
// Patch: Change JE at 0x004100D1 from "74 25" to "EB 1F"
//   This makes the code unconditionally jump to "mov eax, 1; ret" at
//   0x004100F2, always returning success regardless of the DVD check result.
//   Target calc: 0x004100F2 - 0x004100D1 - 2 = 0x1F

static bool PatchDvdCheck() {
  BYTE *addr = (BYTE *)0x004100D1;

  if (addr[0] == 0x74 && addr[1] == 0x25) {
    DWORD oldProtect;
    if (VirtualProtect(addr, 2, PAGE_EXECUTE_READWRITE, &oldProtect)) {
      addr[0] = 0xEB; // JE → JMP (unconditional)
      addr[1] = 0x1F; // jump to 0x004100F2 (mov eax, 1; ret)
      VirtualProtect(addr, 2, oldProtect, &oldProtect);
      FlushInstructionCache(GetCurrentProcess(), addr, 2);
      return true;
    }
  } else if (addr[0] == 0xEB && addr[1] == 0x1F) {
    return true; // Already patched
  }
  return false;
}

static int WINAPI Hook_MessageBoxA(HWND hWnd, LPCSTR lpText, LPCSTR lpCaption,
                                   UINT uType) {
  if (lpText && strstr(lpText, "\x8F\x49")) {
    return Real_MessageBoxA(hWnd, "Apakah anda ingin keluar dari permainan", "DC3DD", uType);
  }
  // Translate Japanese text to Indonesian
  const char *translatedText = TranslateUI(lpText);
  const char *translatedCaption = TranslateUI(lpCaption);
  if (!translatedText)
    translatedText = TranslateUIPartial(lpText);
  if (!translatedCaption)
    translatedCaption = TranslateUIPartial(lpCaption);

  return Real_MessageBoxA(hWnd, translatedText ? translatedText : lpText,
                          translatedCaption ? translatedCaption : lpCaption,
                          uType);
}

static int WINAPI Hook_MessageBoxW(HWND hWnd, LPCWSTR lpText, LPCWSTR lpCaption,
                                   UINT uType) {
  if (lpText && (wcscmp(lpText, L"終了しますか？") == 0 || wcsstr(lpText, L"終了しますか") != nullptr)) {
    return Real_MessageBoxW(hWnd, L"Apakah anda ingin keluar dari permainan", L"DC3DD", uType);
  }
  return Real_MessageBoxW(hWnd, lpText, lpCaption, uType);
}

static int WINAPI Hook_MessageBoxExA(HWND hWnd, LPCSTR lpText, LPCSTR lpCaption,
                                     UINT uType, WORD wLanguageId) {
  if (lpText && strstr(lpText, "\x8F\x49")) {
    return Real_MessageBoxExA(hWnd, "Apakah anda ingin keluar dari permainan",
                              "DC3DD", uType, wLanguageId);
  }
  return Real_MessageBoxExA(hWnd, lpText, lpCaption, uType, wLanguageId);
}

static int WINAPI Hook_MessageBoxExW(HWND hWnd, LPCWSTR lpText, LPCWSTR lpCaption,
                                     UINT uType, WORD wLanguageId) {
  if (lpText && (wcscmp(lpText, L"終了しますか？") == 0 || wcsstr(lpText, L"終了しますか") != nullptr)) {
    return Real_MessageBoxExW(hWnd, L"Apakah anda ingin keluar dari permainan",
                              L"DC3DD", uType, wLanguageId);
  }
  return Real_MessageBoxExW(hWnd, lpText, lpCaption, uType, wLanguageId);
}

static int WINAPI Hook_MessageBoxIndirectA(const MSGBOXPARAMSA *lpmbp) {
  if (!lpmbp) {
    return Real_MessageBoxIndirectA(lpmbp);
  }
  if (!IS_INTRESOURCE(lpmbp->lpszText) && lpmbp->lpszText &&
      strstr(lpmbp->lpszText, "\x8F\x49")) {
    MSGBOXPARAMSA msg = *lpmbp;
    msg.lpszText = "Apakah anda ingin keluar dari permainan";
    msg.lpszCaption = "DC3DD";
    return Real_MessageBoxIndirectA(&msg);
  }
  return Real_MessageBoxIndirectA(lpmbp);
}

static int WINAPI Hook_MessageBoxIndirectW(const MSGBOXPARAMSW *lpmbp) {
  if (!lpmbp) {
    return Real_MessageBoxIndirectW(lpmbp);
  }
  if (!IS_INTRESOURCE(lpmbp->lpszText) && lpmbp->lpszText &&
      (wcscmp(lpmbp->lpszText, L"終了しますか？") == 0 ||
       wcsstr(lpmbp->lpszText, L"終了しますか") != nullptr)) {
    MSGBOXPARAMSW msg = *lpmbp;
    msg.lpszText = L"Apakah anda ingin keluar dari permainan";
    msg.lpszCaption = L"DC3DD";
    return Real_MessageBoxIndirectW(&msg);
  }
  return Real_MessageBoxIndirectW(lpmbp);
}

static const char* TranslateSysMenuItemAndTrackId(const char* lpNewItem, UINT_PTR uIDNewItem) {
  (void)lpNewItem;
  (void)uIDNewItem;
  return nullptr;
}

static const WCHAR* TranslateSysMenuItemAndTrackIdW(const WCHAR* lpNewItem, UINT_PTR uIDNewItem) {
  (void)lpNewItem;
  (void)uIDNewItem;
  return nullptr;
}

// Menu hooks - translate Japanese menu item text to Indonesian
static BOOL WINAPI Hook_AppendMenuA(HMENU hMenu, UINT uFlags,
                                    UINT_PTR uIDNewItem, LPCSTR lpNewItem) {
  if (lpNewItem && !(uFlags & (MF_BITMAP | MF_OWNERDRAW | MF_SEPARATOR))) {
    const char *menuOverride = TranslateSysMenuItemAndTrackId(lpNewItem, uIDNewItem);
    if (menuOverride) {
      return Real_AppendMenuA(hMenu, uFlags, uIDNewItem, menuOverride);
    }
    const char *translated = TranslateUI(lpNewItem);
    if (!translated)
      translated = TranslateUIPartial(lpNewItem);
    if (translated)
      return Real_AppendMenuA(hMenu, uFlags, uIDNewItem, translated);
  }
  return Real_AppendMenuA(hMenu, uFlags, uIDNewItem, lpNewItem);
}

static BOOL WINAPI Hook_InsertMenuA(HMENU hMenu, UINT uPosition, UINT uFlags,
                                    UINT_PTR uIDNewItem, LPCSTR lpNewItem) {
  if (lpNewItem && !(uFlags & (MF_BITMAP | MF_OWNERDRAW | MF_SEPARATOR))) {
    const char *menuOverride = TranslateSysMenuItemAndTrackId(lpNewItem, uIDNewItem);
    if (menuOverride) {
      return Real_InsertMenuA(hMenu, uPosition, uFlags, uIDNewItem, menuOverride);
    }
    const char *translated = TranslateUI(lpNewItem);
    if (!translated)
      translated = TranslateUIPartial(lpNewItem);
    if (translated)
      return Real_InsertMenuA(hMenu, uPosition, uFlags, uIDNewItem, translated);
  }
  return Real_InsertMenuA(hMenu, uPosition, uFlags, uIDNewItem, lpNewItem);
}

static BOOL WINAPI Hook_ModifyMenuA(HMENU hMenu, UINT uPosition, UINT uFlags,
                                    UINT_PTR uIDNewItem, LPCSTR lpNewItem) {
  if (lpNewItem && !(uFlags & (MF_BITMAP | MF_OWNERDRAW | MF_SEPARATOR))) {
    const char *menuOverride = TranslateSysMenuItemAndTrackId(lpNewItem, uIDNewItem);
    if (menuOverride) {
      return Real_ModifyMenuA(hMenu, uPosition, uFlags, uIDNewItem, menuOverride);
    }
    const char *translated = TranslateUI(lpNewItem);
    if (!translated)
      translated = TranslateUIPartial(lpNewItem);
    if (translated)
      return Real_ModifyMenuA(hMenu, uPosition, uFlags, uIDNewItem, translated);
  }
  return Real_ModifyMenuA(hMenu, uPosition, uFlags, uIDNewItem, lpNewItem);
}

static BOOL WINAPI Hook_AppendMenuW(HMENU hMenu, UINT uFlags,
                                    UINT_PTR uIDNewItem, LPCWSTR lpNewItem) {
  if (lpNewItem && !(uFlags & (MF_BITMAP | MF_OWNERDRAW | MF_SEPARATOR))) {
    const WCHAR *menuOverride = TranslateSysMenuItemAndTrackIdW(lpNewItem, uIDNewItem);
    if (menuOverride) {
      return Real_AppendMenuW(hMenu, uFlags, uIDNewItem, menuOverride);
    }
  }
  return Real_AppendMenuW(hMenu, uFlags, uIDNewItem, lpNewItem);
}

static BOOL WINAPI Hook_InsertMenuW(HMENU hMenu, UINT uPosition, UINT uFlags,
                                    UINT_PTR uIDNewItem, LPCWSTR lpNewItem) {
  if (lpNewItem && !(uFlags & (MF_BITMAP | MF_OWNERDRAW | MF_SEPARATOR))) {
    const WCHAR *menuOverride = TranslateSysMenuItemAndTrackIdW(lpNewItem, uIDNewItem);
    if (menuOverride) {
      return Real_InsertMenuW(hMenu, uPosition, uFlags, uIDNewItem, menuOverride);
    }
  }
  return Real_InsertMenuW(hMenu, uPosition, uFlags, uIDNewItem, lpNewItem);
}

static BOOL WINAPI Hook_ModifyMenuW(HMENU hMenu, UINT uPosition, UINT uFlags,
                                    UINT_PTR uIDNewItem, LPCWSTR lpNewItem) {
  if (lpNewItem && !(uFlags & (MF_BITMAP | MF_OWNERDRAW | MF_SEPARATOR))) {
    const WCHAR *menuOverride = TranslateSysMenuItemAndTrackIdW(lpNewItem, uIDNewItem);
    if (menuOverride) {
      return Real_ModifyMenuW(hMenu, uPosition, uFlags, uIDNewItem, menuOverride);
    }
  }
  return Real_ModifyMenuW(hMenu, uPosition, uFlags, uIDNewItem, lpNewItem);
}

static BOOL WINAPI Hook_TrackPopupMenu(HMENU hMenu, UINT uFlags, int x, int y,
                                       int nReserved, HWND hWnd,
                                       const RECT *prcRect) {
  RelabelSystemMenu(hMenu);
  return Real_TrackPopupMenu(hMenu, uFlags, x, y, nReserved, hWnd, prcRect);
}

static BOOL WINAPI Hook_TrackPopupMenuEx(HMENU hMenu, UINT uFlags, int x, int y,
                                         HWND hwnd, LPTPMPARAMS lptpm) {
  RelabelSystemMenu(hMenu);
  return Real_TrackPopupMenuEx(hMenu, uFlags, x, y, hwnd, lptpm);
}

// ============================================================================
// KEY (Serial Key) Verification Bypass - Skip startup dialogs
// ============================================================================
static int g_dialogSkipCount = 0;

static INT_PTR WINAPI Hook_DialogBoxParamA(HINSTANCE hInstance,
                                           LPCSTR lpTemplateName,
                                           HWND hWndParent,
                                           DLGPROC lpDialogFunc,
                                           LPARAM dwInitParam) {
  return Real_DialogBoxParamA(hInstance, lpTemplateName, hWndParent,
                              lpDialogFunc, dwInitParam);
}

// ============================================================================
// Settings Dialog
// ============================================================================

#define IDC_CHECK_BACKLOG_ICONS 1001
#define IDC_CHECK_FILE_REDIRECT 1002
#define IDC_BTN_CHANGE_DIALOGUE_FONT 1003
#define IDC_BTN_CHANGE_BACKLOG_FONT 1006
#define IDC_LBL_SPACING 1007
#define IDC_EDIT_SPACING 1008
#define IDC_LBL_XOFFSET 1009
#define IDC_EDIT_XOFFSET 1010
#define IDC_LBL_YOFFSET 1011
#define IDC_EDIT_YOFFSET 1012
#define IDC_BTN_CHANGE_BACKLOG_NAME_FONT 1013
#define IDC_LBL_NAME_XOFFSET 1020
#define IDC_EDIT_NAME_XOFFSET 1021
#define IDC_LBL_NAME_YOFFSET 1018
#define IDC_EDIT_NAME_YOFFSET 1019
#define IDC_LBL_NAME_SPACING 1014
#define IDC_EDIT_NAME_SPACING 1015
#define IDC_LBL_DIALOG_SPACING 1016
#define IDC_EDIT_DIALOG_SPACING 1017
#define IDC_CHK_ADVANCED 1022
#define IDC_BTN_OK 1004
#define IDC_BTN_CANCEL 1005
#define IDC_BTN_ABOUT 1023
#define IDC_BTN_ADVANCED 1024
#define IDC_COMBO_LANGUAGE 1026
#define IDC_EDIT_DIALOGUE_SIZE 1030
#define IDC_LBL_DIALOGUE_SIZE 1031
#define IDC_EDIT_BACKLOG_SIZE 1032
#define IDC_LBL_BACKLOG_SIZE 1033
#define IDC_LBL_DIALOGUE_LINE_SPACING 1034
#define IDC_EDIT_DIALOGUE_LINE_SPACING 1035
#define IDC_LBL_DIALOGUE_XOFFSET 1036
#define IDC_EDIT_DIALOGUE_XOFFSET 1037
#define IDC_LBL_DIALOGUE_YOFFSET 1038
#define IDC_EDIT_DIALOGUE_YOFFSET 1039
#define IDC_LBL_DIALOGUE_SPACING 1040
#define IDC_EDIT_DIALOGUE_SPACING 1041

static INT_PTR CALLBACK AdvancedSettingsDialogProc(HWND hwndDlg, UINT uMsg,
                                                   WPARAM wParam, LPARAM lParam);
static void ShowAdvancedSettingsDialog(HWND parent);

static INT_PTR CALLBACK SettingsDialogProc(HWND hwndDlg, UINT uMsg,
                                           WPARAM wParam, LPARAM lParam) {
  switch (uMsg) {
  case WM_INITDIALOG: {
    // Center the dialog on the parent window
    RECT rcParent, rcDlg;
    GetWindowRect(g_mainWindow, &rcParent);
    GetWindowRect(hwndDlg, &rcDlg);
    int x = rcParent.left +
            (rcParent.right - rcParent.left - (rcDlg.right - rcDlg.left)) / 2;
    int y = rcParent.top +
            (rcParent.bottom - rcParent.top - (rcDlg.bottom - rcDlg.top)) / 2;
    SetWindowPos(hwndDlg, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

    // Populate ComboBox
    HWND hCombo = GetDlgItem(hwndDlg, IDC_COMBO_LANGUAGE);
    SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)L"Japanese (Original)");
    SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)L"Indonesian / Bahasa Indonesia (id_Data)");
    SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)L"English (eng_data)");
    
    // Set selection
    SendMessageW(hCombo, CB_SETCURSEL, g_fontManager.GetLanguage(), 0);


    return TRUE;
  }

  case WM_COMMAND: {
    if (LOWORD(wParam) == IDC_BTN_ADVANCED) {
      ShowAdvancedSettingsDialog(hwndDlg);
      return TRUE;
    }

    switch (LOWORD(wParam)) {
    case IDC_BTN_CHANGE_DIALOGUE_FONT: {
      // Show font chooser dialog
      LOGFONTW lf = {};
      CHOOSEFONTW cf = {};
      cf.lStructSize = sizeof(cf);
      cf.hwndOwner = hwndDlg;
      cf.lpLogFont = &lf;
      cf.Flags = CF_SCREENFONTS | CF_INITTOLOGFONTSTRUCT;
      lf.lfCharSet = DEFAULT_CHARSET;
      wcscpy_s(lf.lfFaceName, g_fontManager.GetDialogueFontName().c_str());
      lf.lfHeight = g_fontManager.GetDialogueFontSize();

      g_inChooseFont = true;
      if (ChooseFontW(&cf)) {
        int height = lf.lfHeight;
        g_fontManager.SetDialogueFont(lf.lfFaceName, height);
        g_inChooseFont = false;
        MessageBoxW(hwndDlg, L"Dialogue font updated!", L"DC3DD Patch",
                    MB_OK | MB_ICONINFORMATION);
      } else {
        g_inChooseFont = false;
      }
      return TRUE;
    }

    case IDC_BTN_CHANGE_BACKLOG_FONT: {
      // Show font chooser dialog
      LOGFONTW lf = {};
      CHOOSEFONTW cf = {};
      cf.lStructSize = sizeof(cf);
      cf.hwndOwner = hwndDlg;
      cf.lpLogFont = &lf;
      cf.Flags = CF_SCREENFONTS | CF_INITTOLOGFONTSTRUCT;
      lf.lfCharSet = DEFAULT_CHARSET;
      wcscpy_s(lf.lfFaceName, g_fontManager.GetBacklogFontName().c_str());
      lf.lfHeight = g_fontManager.GetBacklogFontSize();

      g_inChooseFont = true;
      if (ChooseFontW(&cf)) {
        int height = lf.lfHeight;
        g_fontManager.SetBacklogFont(lf.lfFaceName, height);
        g_inChooseFont = false;
        MessageBoxW(hwndDlg, L"Backlog font updated!", L"DC3DD Patch",
                    MB_OK | MB_ICONINFORMATION);
      } else {
        g_inChooseFont = false;
      }
      return TRUE;
    }

    case IDC_BTN_CHANGE_BACKLOG_NAME_FONT: {
      // Show font chooser dialog
      LOGFONTW lf = {};
      CHOOSEFONTW cf = {};
      cf.lStructSize = sizeof(cf);
      cf.hwndOwner = hwndDlg;
      cf.lpLogFont = &lf;
      cf.Flags = CF_SCREENFONTS | CF_INITTOLOGFONTSTRUCT;
      lf.lfCharSet = DEFAULT_CHARSET;
      wcscpy_s(lf.lfFaceName, g_fontManager.GetBacklogNameFontName().c_str());
      lf.lfHeight = g_fontManager.GetBacklogNameFontSize();

      g_inChooseFont = true;
      if (ChooseFontW(&cf)) {
        int height = lf.lfHeight;
        g_fontManager.SetBacklogNameFont(lf.lfFaceName, height);
        g_inChooseFont = false;
        MessageBoxW(hwndDlg, L"Backlog Character Name font updated!",
                    L"DC3DD Patch", MB_OK | MB_ICONINFORMATION);
      } else {
        g_inChooseFont = false;
      }
      return TRUE;
    }

    case IDC_BTN_OK: {
      // Save language settings
      HWND hCombo = GetDlgItem(hwndDlg, IDC_COMBO_LANGUAGE);
      int selectedLang = (int)SendMessageW(hCombo, CB_GETCURSEL, 0, 0);
      if (selectedLang == CB_ERR) selectedLang = 0;

      bool anyChanged = (selectedLang != g_fontManager.GetLanguage());

      if (anyChanged) {
        g_fontManager.SetLanguage(selectedLang);
        MessageBoxW(hwndDlg,
                    L"\u26a0\ufe0f File redirection setting changed.\n\n"
                    L"Return to title screen or restart the game\n"
                    L"for this to take full effect.",
                    L"DC3DD Patch", MB_OK | MB_ICONWARNING);
      }

      EndDialog(hwndDlg, IDOK);
      return TRUE;
    }

    case IDC_BTN_ABOUT:
      MessageBoxW(
          hwndDlg,
          L"This patch has been made by:\nSakura Symphony Re;Translation",
          L"About DC3DD Patch", MB_OK | MB_ICONINFORMATION);
      return TRUE;

    case IDC_BTN_CANCEL:
      EndDialog(hwndDlg, IDCANCEL);
      return TRUE;
    }
    break;
  }

  case WM_CLOSE:
    EndDialog(hwndDlg, IDCANCEL);
    return TRUE;
  }
  return FALSE;
}

// In-memory DIALOG resource construction
static void ShowSettingsDialog() {
  TraceLog("ShowSettingsDialog enter g_mainWindow=%p", g_mainWindow);
  if (!g_mainWindow)
    return;

  // Allocate a buffer for the DLGTEMPLATE and controls
  const int bufSize = 4096;
  BYTE *buffer = new BYTE[bufSize];
  memset(buffer, 0, bufSize);

  WORD *pw = (WORD *)buffer;

  // Dialog Header
  DLGTEMPLATE *pDlg = (DLGTEMPLATE *)pw;
  pDlg->style = WS_POPUP | WS_BORDER | WS_SYSMENU | WS_CAPTION | DS_MODALFRAME |
                DS_CENTER | DS_SETFONT;
  pDlg->dwExtendedStyle = 0;
  pDlg->cdit = 11; // 2 Warning Labels, 2 Checkboxes, 3 Font Buttons, 1 Adv Button, OK, Cancel, About
  pDlg->x = 0;
  pDlg->y = 0;
  pDlg->cx = 250;
  pDlg->cy = 200;

  pw = (WORD *)(pDlg + 1);
  *pw++ = 0; // Menu
  *pw++ = 0; // Class
  // Title: null-terminated Unicode string
  const WCHAR *dlgTitle = L"DC3DD Patch Settings";
  wcscpy((WCHAR *)pw, dlgTitle);
  pw += wcslen(dlgTitle) + 1;
  // Font (because DS_SETFONT): point size + font name
  *pw++ = 9; // point size
  const WCHAR *fontName = L"Segoe UI";
  wcscpy((WCHAR *)pw, fontName);
  pw += wcslen(fontName) + 1;

  // Helper: align pointer to DWORD boundary (required before each
  // DLGITEMTEMPLATE)
  auto AlignDword = [](WORD *&p) {
    while ((ULONG_PTR)p & 3)
      p++;
  };

  // Control: Warning Label English
  AlignDword(pw);
  DLGITEMTEMPLATE *pItem = (DLGITEMTEMPLATE *)pw;
  pItem->style = WS_CHILD | WS_VISIBLE | SS_LEFT;
  pItem->dwExtendedStyle = 0;
  pItem->x = 10;
  pItem->y = 10;
  pItem->cx = 230;
  pItem->cy = 10;
  pItem->id = -1;
  pw = (WORD *)(pItem + 1);
  *pw++ = 0xFFFF;
  *pw++ = 0x0082; // Static class
  const WCHAR *textWarnEn = L"Warning: Wrong setting could lead to crashing.";
  wcscpy((WCHAR *)pw, textWarnEn);
  pw += wcslen(textWarnEn) + 1;
  *pw++ = 0;

  // Control: Warning Label Indonesian
  AlignDword(pw);
  pItem = (DLGITEMTEMPLATE *)pw;
  pItem->style = WS_CHILD | WS_VISIBLE | SS_LEFT;
  pItem->dwExtendedStyle = 0;
  pItem->x = 10;
  pItem->y = 20;
  pItem->cx = 230;
  pItem->cy = 10;
  pItem->id = -1;
  pw = (WORD *)(pItem + 1);
  *pw++ = 0xFFFF;
  *pw++ = 0x0082; // Static class
  const WCHAR *textWarnId = L"Peringatan: Pengaturan yang salah dapat menyebabkan crash.";
  wcscpy((WCHAR *)pw, textWarnId);
  pw += wcslen(textWarnId) + 1;
  *pw++ = 0;

  // Control 1: Language Label
  AlignDword(pw);
  pItem = (DLGITEMTEMPLATE *)pw;
  pItem->style = WS_CHILD | WS_VISIBLE | SS_LEFT;
  pItem->dwExtendedStyle = 0;
  pItem->x = 10;
  pItem->y = 35;
  pItem->cx = 230;
  pItem->cy = 10;
  pItem->id = -1;
  pw = (WORD *)(pItem + 1);
  *pw++ = 0xFFFF;
  *pw++ = 0x0082; // Static class
  const WCHAR *text2 = L"Game Text Language / Bahasa / \x8A00\x8A9E:";
  wcscpy((WCHAR *)pw, text2);
  pw += wcslen(text2) + 1;
  *pw++ = 0;

  // Control 1.5: Language ComboBox
  AlignDword(pw);
  pItem = (DLGITEMTEMPLATE *)pw;
  pItem->style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL;
  pItem->dwExtendedStyle = 0;
  pItem->x = 10;
  pItem->y = 48;
  pItem->cx = 200;
  pItem->cy = 100; // Dropdown height
  pItem->id = IDC_COMBO_LANGUAGE;
  pw = (WORD *)(pItem + 1);
  *pw++ = 0xFFFF;
  *pw++ = 0x0085; // ComboBox class
  *pw++ = 0; // Empty title
  *pw++ = 0; // No creation data

  // Control 3: Change Dialogue Font Button
  AlignDword(pw);
  pItem = (DLGITEMTEMPLATE *)pw;
  pItem->style = WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON;
  pItem->dwExtendedStyle = 0;
  pItem->x = 10;
  pItem->y = 72;
  pItem->cx = 200;
  pItem->cy = 18;
  pItem->id = IDC_BTN_CHANGE_DIALOGUE_FONT;
  pw = (WORD *)(pItem + 1);
  *pw++ = 0xFFFF;
  *pw++ = 0x0080;
  const WCHAR *text3 = L"Change Dialogue Font...";
  wcscpy((WCHAR *)pw, text3);
  pw += wcslen(text3) + 1;
  *pw++ = 0;

  // Control 3.5: Change Backlog Font Button
  AlignDword(pw);
  pItem = (DLGITEMTEMPLATE *)pw;
  pItem->style = WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON;
  pItem->dwExtendedStyle = 0;
  pItem->x = 10;
  pItem->y = 92;
  pItem->cx = 200;
  pItem->cy = 18;
  pItem->id = IDC_BTN_CHANGE_BACKLOG_FONT;
  pw = (WORD *)(pItem + 1);
  *pw++ = 0xFFFF;
  *pw++ = 0x0080;
  const WCHAR *text35 = L"Change Backlog Font...";
  wcscpy((WCHAR *)pw, text35);
  pw += wcslen(text35) + 1;
  *pw++ = 0;

  // Control 3.75: Change Backlog Name Font Button
  AlignDword(pw);
  pItem = (DLGITEMTEMPLATE *)pw;
  pItem->style = WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON;
  pItem->dwExtendedStyle = 0;
  pItem->x = 10;
  pItem->y = 112;
  pItem->cx = 200;
  pItem->cy = 18;
  pItem->id = IDC_BTN_CHANGE_BACKLOG_NAME_FONT;
  pw = (WORD *)(pItem + 1);
  *pw++ = 0xFFFF;
  *pw++ = 0x0080;
  const WCHAR *text375 = L"Change Backlog Name Font...";
  wcscpy((WCHAR *)pw, text375);
  pw += wcslen(text375) + 1;
  *pw++ = 0;

  // Control: Advanced Settings Button
  AlignDword(pw);
  pItem = (DLGITEMTEMPLATE *)pw;
  pItem->style = WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON;
  pItem->dwExtendedStyle = 0;
  pItem->x = 10;
  pItem->y = 132;
  pItem->cx = 200;
  pItem->cy = 18;
  pItem->id = IDC_BTN_ADVANCED;
  pw = (WORD *)(pItem + 1);
  *pw++ = 0xFFFF;
  *pw++ = 0x0080; // Button class
  const WCHAR *textAdv = L"Advanced Settings...";
  wcscpy((WCHAR *)pw, textAdv);
  pw += wcslen(textAdv) + 1;
  *pw++ = 0;

  // Control 4: OK Button
  AlignDword(pw);
  pItem = (DLGITEMTEMPLATE *)pw;
  pItem->style = WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP;
  pItem->dwExtendedStyle = 0;
  pItem->x = 50;
  pItem->y = 162;
  pItem->cx = 50;
  pItem->cy = 18;
  pItem->id = IDC_BTN_OK;
  pw = (WORD *)(pItem + 1);
  *pw++ = 0xFFFF;
  *pw++ = 0x0080;
  const WCHAR *text4 = L"OK";
  wcscpy((WCHAR *)pw, text4);
  pw += wcslen(text4) + 1;
  *pw++ = 0;

  // Control 5: Cancel Button
  AlignDword(pw);
  pItem = (DLGITEMTEMPLATE *)pw;
  pItem->style = WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP;
  pItem->dwExtendedStyle = 0;
  pItem->x = 110;
  pItem->y = 162;
  pItem->cx = 50;
  pItem->cy = 18;
  pItem->id = IDC_BTN_CANCEL;
  pw = (WORD *)(pItem + 1);
  *pw++ = 0xFFFF;
  *pw++ = 0x0080;
  const WCHAR *text5 = L"Cancel";
  wcscpy((WCHAR *)pw, text5);
  pw += wcslen(text5) + 1;
  *pw++ = 0;

  // Control 6: About Button
  AlignDword(pw);
  pItem = (DLGITEMTEMPLATE *)pw;
  pItem->style = WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP;
  pItem->dwExtendedStyle = 0;
  pItem->x = 170;
  pItem->y = 162;
  pItem->cx = 50;
  pItem->cy = 18;
  pItem->id = IDC_BTN_ABOUT;
  pw = (WORD *)(pItem + 1);
  *pw++ = 0xFFFF;
  *pw++ = 0x0080;
  const WCHAR *text6 = L"About";
  wcscpy((WCHAR *)pw, text6);
  pw += wcslen(text6) + 1;
  *pw++ = 0;

  // Show dialog
  INT_PTR dlgRet = DialogBoxIndirectW(GetModuleHandleA(NULL), (DLGTEMPLATE *)buffer,
                                      g_mainWindow, SettingsDialogProc);
  TraceLog("ShowSettingsDialog ret=%Id lastErr=%lu", dlgRet, GetLastError());
  delete[] buffer;
}

static INT_PTR CALLBACK AdvancedSettingsDialogProc(HWND hwndDlg, UINT uMsg,
                                                   WPARAM wParam, LPARAM lParam) {
  switch (uMsg) {
  case WM_INITDIALOG: {
    RECT rcParent, rcDlg;
    GetWindowRect(GetParent(hwndDlg), &rcParent);
    GetWindowRect(hwndDlg, &rcDlg);
    int x = rcParent.left + (rcParent.right - rcParent.left - (rcDlg.right - rcDlg.left)) / 2;
    int y = rcParent.top + (rcParent.bottom - rcParent.top - (rcDlg.bottom - rcDlg.top)) / 2;
    SetWindowPos(hwndDlg, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

    SetDlgItemInt(hwndDlg, IDC_EDIT_SPACING, g_fontManager.GetBacklogLineSpacing(), TRUE);
    SetDlgItemInt(hwndDlg, IDC_EDIT_XOFFSET, g_fontManager.GetBacklogXOffset(), TRUE);
    SetDlgItemInt(hwndDlg, IDC_EDIT_YOFFSET, g_fontManager.GetBacklogYOffset(), TRUE);
    SetDlgItemInt(hwndDlg, IDC_EDIT_NAME_XOFFSET, g_fontManager.GetBacklogNameXOffset(), TRUE);
    SetDlgItemInt(hwndDlg, IDC_EDIT_NAME_YOFFSET, g_fontManager.GetBacklogNameYOffset(), TRUE);
    SetDlgItemInt(hwndDlg, IDC_EDIT_NAME_SPACING, g_fontManager.GetBacklogNameSpacing(), TRUE);
    SetDlgItemInt(hwndDlg, IDC_EDIT_DIALOG_SPACING, g_fontManager.GetBacklogDialogSpacing(), TRUE);
    SetDlgItemInt(hwndDlg, IDC_EDIT_DIALOGUE_XOFFSET, g_fontManager.GetDialogueXOffset(), TRUE);
    SetDlgItemInt(hwndDlg, IDC_EDIT_DIALOGUE_YOFFSET, g_fontManager.GetDialogueYOffset(), TRUE);
    SetDlgItemInt(hwndDlg, IDC_EDIT_DIALOGUE_SPACING, g_fontManager.GetDialogueLineSpacing(), TRUE);
    return TRUE;
  }

  case WM_COMMAND: {
    if (LOWORD(wParam) == IDC_BTN_OK) {
      int spacing = g_fontManager.GetBacklogLineSpacing(); // Moved to main menu
      int xOffset = GetDlgItemInt(hwndDlg, IDC_EDIT_XOFFSET, NULL, TRUE);
      int yOffset = GetDlgItemInt(hwndDlg, IDC_EDIT_YOFFSET, NULL, TRUE);
      int nameXOffset = GetDlgItemInt(hwndDlg, IDC_EDIT_NAME_XOFFSET, NULL, TRUE);
      int nameYOffset = GetDlgItemInt(hwndDlg, IDC_EDIT_NAME_YOFFSET, NULL, TRUE);
      int nameSpacing = GetDlgItemInt(hwndDlg, IDC_EDIT_NAME_SPACING, NULL, TRUE);
      int dialogSpacing = GetDlgItemInt(hwndDlg, IDC_EDIT_DIALOG_SPACING, NULL, TRUE);
      
      int diagXOffset = GetDlgItemInt(hwndDlg, IDC_EDIT_DIALOGUE_XOFFSET, NULL, TRUE);
      int diagYOffset = GetDlgItemInt(hwndDlg, IDC_EDIT_DIALOGUE_YOFFSET, NULL, TRUE);
      int diagSpacing = GetDlgItemInt(hwndDlg, IDC_EDIT_DIALOGUE_SPACING, NULL, TRUE);

      g_fontManager.SetBacklogOffsets(xOffset, yOffset, spacing, nameXOffset,
                                      nameYOffset, nameSpacing, dialogSpacing, diagSpacing);
      
      g_fontManager.SetDialogueOffsets(diagXOffset, diagYOffset);

      g_fontManager.SetAdvancedSettings(true); // Implicitly enabled when used
      EndDialog(hwndDlg, IDOK);
      return TRUE;
    } else if (LOWORD(wParam) == IDC_BTN_CANCEL) {
      EndDialog(hwndDlg, IDCANCEL);
      return TRUE;
    }
    break;
  }

  case WM_CLOSE:
    EndDialog(hwndDlg, IDCANCEL);
    return TRUE;
  }
  return FALSE;
}

static void ShowAdvancedSettingsDialog(HWND parent) {
  const int bufSize = 4096;
  BYTE *buffer = new BYTE[bufSize];
  memset(buffer, 0, bufSize);
  WORD *pw = (WORD *)buffer;

  DLGTEMPLATE *pDlg = (DLGTEMPLATE *)pw;
  pDlg->style = WS_POPUP | WS_BORDER | WS_SYSMENU | WS_CAPTION | DS_MODALFRAME | DS_CENTER | DS_SETFONT;
  pDlg->cdit = 20; // 9 labels, 9 edits, 2 buttons
  pDlg->cx = 200;
  pDlg->cy = 260;

  pw = (WORD *)(pDlg + 1);
  *pw++ = 0; *pw++ = 0;
  wcscpy((WCHAR *)pw, L"Advanced Settings");
  pw += wcslen((WCHAR *)pw) + 1;
  *pw++ = 9;
  wcscpy((WCHAR *)pw, L"Segoe UI");
  pw += wcslen((WCHAR *)pw) + 1;

  auto AlignDword = [](WORD *&p) { while ((ULONG_PTR)p & 3) p++; };
  auto AddLabelAndEdit = [&](int idLbl, int idEdit, const WCHAR* labelText, int yPos) {
    AlignDword(pw);
    DLGITEMTEMPLATE *pItem = (DLGITEMTEMPLATE *)pw;
    pItem->style = WS_CHILD | WS_VISIBLE | SS_LEFT;
    pItem->x = 10; pItem->y = yPos + 2; pItem->cx = 100; pItem->cy = 14; pItem->id = idLbl;
    pw = (WORD *)(pItem + 1); *pw++ = 0xFFFF; *pw++ = 0x0082;
    wcscpy((WCHAR *)pw, labelText); pw += wcslen(labelText) + 1; *pw++ = 0;

    AlignDword(pw);
    pItem = (DLGITEMTEMPLATE *)pw;
    pItem->style = WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL;
    pItem->dwExtendedStyle = WS_EX_CLIENTEDGE;
    pItem->x = 115; pItem->y = yPos; pItem->cx = 40; pItem->cy = 14; pItem->id = idEdit;
    pw = (WORD *)(pItem + 1); *pw++ = 0xFFFF; *pw++ = 0x0081;
    wcscpy((WCHAR *)pw, L""); pw += wcslen(L"") + 1; *pw++ = 0;
  };

  // Line spacing moved to main menu
  AddLabelAndEdit(IDC_LBL_XOFFSET, IDC_EDIT_XOFFSET, L"Backlog X Offset:", 30);
  AddLabelAndEdit(IDC_LBL_YOFFSET, IDC_EDIT_YOFFSET, L"Backlog Y Offset:", 50);
  AddLabelAndEdit(IDC_LBL_NAME_XOFFSET, IDC_EDIT_NAME_XOFFSET, L"Name X Offset:", 70);
  AddLabelAndEdit(IDC_LBL_NAME_YOFFSET, IDC_EDIT_NAME_YOFFSET, L"Name Y Offset:", 90);
  AddLabelAndEdit(IDC_LBL_NAME_SPACING, IDC_EDIT_NAME_SPACING, L"Name Ext Spacing:", 110);
  AddLabelAndEdit(IDC_LBL_DIALOG_SPACING, IDC_EDIT_DIALOG_SPACING, L"Base Ext Spacing:", 130);
  AddLabelAndEdit(IDC_LBL_DIALOGUE_XOFFSET, IDC_EDIT_DIALOGUE_XOFFSET, L"Dialogue X Offset:", 150);
  AddLabelAndEdit(IDC_LBL_DIALOGUE_YOFFSET, IDC_EDIT_DIALOGUE_YOFFSET, L"Dialogue Y Offset:", 170);
  AddLabelAndEdit(IDC_LBL_DIALOGUE_SPACING, IDC_EDIT_DIALOGUE_SPACING, L"Dialogue Line Spacing:", 190);

  AlignDword(pw);
  DLGITEMTEMPLATE *pItem = (DLGITEMTEMPLATE *)pw;
  pItem->style = WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP;
  pItem->x = 30; pItem->y = 220; pItem->cx = 50; pItem->cy = 16; pItem->id = IDC_BTN_OK;
  pw = (WORD *)(pItem + 1); *pw++ = 0xFFFF; *pw++ = 0x0080;
  wcscpy((WCHAR *)pw, L"OK"); pw += wcslen(L"OK") + 1; *pw++ = 0;

  AlignDword(pw);
  pItem = (DLGITEMTEMPLATE *)pw;
  pItem->style = WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP;
  pItem->x = 100; pItem->y = 220; pItem->cx = 50; pItem->cy = 16; pItem->id = IDC_BTN_CANCEL;
  pw = (WORD *)(pItem + 1); *pw++ = 0xFFFF; *pw++ = 0x0080;
  wcscpy((WCHAR *)pw, L"Cancel"); pw += wcslen(L"Cancel") + 1; *pw++ = 0;

  DialogBoxIndirectW(GetModuleHandleA(NULL), (DLGTEMPLATE *)buffer, parent, AdvancedSettingsDialogProc);
  delete[] buffer;
}

// Window procedure hook to add menu item and keyboard shortcut
static LRESULT CALLBACK MenuWndProc(HWND hWnd, UINT uMsg, WPARAM wParam,
                                    LPARAM lParam);

static HWND WINAPI Hook_CreateWindowExA(DWORD dwExStyle, LPCSTR lpClassName,
                                        LPCSTR lpWindowName, DWORD dwStyle,
                                        int X, int Y, int nWidth, int nHeight,
                                        HWND hWndParent, HMENU hMenu,
                                        HINSTANCE hInstance, LPVOID lpParam) {

  if (lpWindowName && strstr(lpWindowName, "D.C.") != NULL) {
      lpWindowName = "D.C. III ~Da Capo III~ Dream Days";
  }

  HWND hWnd = Real_CreateWindowExA(dwExStyle, lpClassName, lpWindowName,
                                   dwStyle, X, Y, nWidth, nHeight, hWndParent,
                                   hMenu, hInstance, lpParam);
  HookMainWindowIfNeeded(hWnd, hWndParent);
  return hWnd;
}

static LRESULT CALLBACK MenuWndProc(HWND hWnd, UINT uMsg, WPARAM wParam,
                                    LPARAM lParam) {
  WNDPROC prevWndProc = (WNDPROC)GetPropW(hWnd, kPrevWndProcProp);
  if (uMsg == WM_INITMENUPOPUP || uMsg == WM_INITMENU || uMsg == WM_SYSCOMMAND || (uMsg == WM_KEYDOWN && wParam == VK_F11)) {
    TraceLog("MenuWndProc hwnd=%p msg=0x%X wParam=0x%IX", hWnd, uMsg, (UINT_PTR)wParam);
  }

  if (uMsg == WM_INITMENUPOPUP) {
    HMENU hMenu = (HMENU)wParam;
    HMENU hSysMenu = GetSystemMenu(hWnd, FALSE);
    if (hMenu == hSysMenu) {
      RelabelSystemMenu(hSysMenu);
    }
  }
  if (uMsg == WM_INITMENU) {
    RelabelSystemMenu((HMENU)wParam);
  }

  // Handle F11 key press
  if (uMsg == WM_KEYDOWN && wParam == VK_F11) {
    ShowSettingsDialog();
    return 0;
  }

  if (uMsg == WM_SYSCOMMAND) {
    const UINT_PTR cmd = ((UINT_PTR)wParam) & 0xFFF0;
    if (g_cmdRestoreWindowSize != 0 &&
        cmd == (g_cmdRestoreWindowSize & 0xFFF0)) {
      ShowVersionInfoDialog(hWnd);
      return 0;
    }

    if (cmd == (kCmdPatchSettings & 0xFFF0)) {
      ShowSettingsDialog();
      return 0;
    }

    if (g_cmdVersionInfo != 0 && cmd == (g_cmdVersionInfo & 0xFFF0)) {
      ShowSettingsDialog();
      return 0;
    }

    // Legacy individual handlers (kept for backwards compatibility)
    if (cmd == (0x114514 & 0xFFF0)) {
      // Show font chooser dialog directly
      LOGFONTW lf = {};
      CHOOSEFONTW cf = {};
      cf.lStructSize = sizeof(cf);
      cf.hwndOwner = hWnd;
      cf.lpLogFont = &lf;
      cf.Flags = CF_SCREENFONTS | CF_INITTOLOGFONTSTRUCT;
      wcscpy_s(lf.lfFaceName, g_fontManager.GetDialogueFontName().c_str());
      lf.lfHeight = g_fontManager.GetDialogueFontSize();

      if (ChooseFontW(&cf)) {
        int height = lf.lfHeight;
        g_fontManager.SetDialogueFont(lf.lfFaceName, height);
        MessageBoxW(hWnd, L"Font updated!", L"DC3DD Patch",
                    MB_OK | MB_ICONINFORMATION);
      }
      return 0;
    }

    if (cmd == (0x1919810 & 0xFFF0)) {
      // Toggle backlog show all icons
      HMENU hSysMenu = GetSystemMenu(hWnd, FALSE);
      if (hSysMenu) {
        MENUITEMINFOW mii = {};
        mii.cbSize = sizeof(MENUITEMINFOW);
        mii.fMask = MIIM_STATE;
        GetMenuItemInfoW(hSysMenu, 0x1919810, FALSE, &mii);

        // Toggle the state
        bool isEnabled = !(mii.fState & MF_CHECKED);
        g_fontManager.SetEnableBacklogAllIcon(isEnabled);

        ModifyMenuW(hSysMenu, 0x1919810,
                    isEnabled ? MF_CHECKED : MF_UNCHECKED,
                    0x1919810, L"Backlog Show All Icons");
      }
      return 0;
    }

    if (cmd == (0x1919811 & 0xFFF0)) {
      // Toggle file redirection
      HMENU hSysMenu = GetSystemMenu(hWnd, FALSE);
      if (hSysMenu) {
        MENUITEMINFOW mii = {};
        mii.cbSize = sizeof(MENUITEMINFOW);
        mii.fMask = MIIM_STATE;
        GetMenuItemInfoW(hSysMenu, 0x1919811, FALSE, &mii);

        bool isEnabled = !(mii.fState & MF_CHECKED);
        g_fontManager.SetLanguage(isEnabled ? 1 : 0);

        ModifyMenuW(hSysMenu, 0x1919811,
                    isEnabled ? MF_CHECKED : MF_UNCHECKED,
                    0x1919811, L"Enable id_Data File Redirection");

        MessageBoxW(
            hWnd,
            isEnabled
                ? L"File redirection ENABLED.\nTranslated files from "
                  L"id_Data\\ "
                  L"will be used.\n\n⚠️ RESTART REQUIRED:\nReturn to title "
                  L"screen or restart the game for this to take full "
                  L"effect.\n(Already-loaded files are cached in memory.)"
                : L"File redirection DISABLED.\nOriginal files from "
                  L"AdvData\\ "
                  L"will be used.\n\n⚠️ RESTART REQUIRED:\nReturn to title "
                  L"screen or restart the game for this to take full "
                  L"effect.\n(Already-loaded files are cached in memory.)",
            L"DC3DD Patch", MB_OK | MB_ICONWARNING);
      }
      return 0;
    }
  }
  if (uMsg == WM_NCDESTROY) {
    if (prevWndProc) {
      SetWindowLongPtrW(hWnd, GWLP_WNDPROC, (LONG_PTR)prevWndProc);
      RemovePropW(hWnd, kPrevWndProcProp);
    }
    if (hWnd == g_mainWindow) {
      g_mainWindow = nullptr;
    }
  }

  if (prevWndProc) {
    return IsWindowUnicode(hWnd)
               ? CallWindowProcW(prevWndProc, hWnd, uMsg, wParam, lParam)
               : CallWindowProcA(prevWndProc, hWnd, uMsg, wParam, lParam);
  }
  return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

// ============================================================================
// IAT (Import Address Table) Patching
// Safer than Detours for system APIs - patches the game's import table
// directly
// ============================================================================
static bool PatchIAT(HMODULE hModule, const char *dllName, PROC oldFunc,
                     PROC newFunc) {
  BYTE *base = (BYTE *)hModule;
  IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)base;
  if (dos->e_magic != IMAGE_DOS_SIGNATURE)
    return false;

  IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(base + dos->e_lfanew);
  if (nt->Signature != IMAGE_NT_SIGNATURE)
    return false;

  IMAGE_IMPORT_DESCRIPTOR *imp =
      (IMAGE_IMPORT_DESCRIPTOR
           *)(base +
              nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT]
                  .VirtualAddress);

  for (; imp->Name; imp++) {
    const char *name = (const char *)(base + imp->Name);
    if (_stricmp(name, dllName) != 0)
      continue;

    IMAGE_THUNK_DATA *thunk = (IMAGE_THUNK_DATA *)(base + imp->FirstThunk);
    for (; thunk->u1.Function; thunk++) {
      PROC *funcPtr = (PROC *)&thunk->u1.Function;
      if (*funcPtr == oldFunc) {
        DWORD oldProtect;
        VirtualProtect(funcPtr, sizeof(PROC), PAGE_READWRITE, &oldProtect);
        *funcPtr = newFunc;
        VirtualProtect(funcPtr, sizeof(PROC), oldProtect, &oldProtect);
        return true;
      }
    }
  }
  return false;
}

// ============================================================================
// Crash Logger (Vectored Exception Handler)
// ============================================================================
static LONG WINAPI CrashLogger(EXCEPTION_POINTERS *ep) {
  DWORD code = ep->ExceptionRecord->ExceptionCode;
  if (code == EXCEPTION_ACCESS_VIOLATION || code == EXCEPTION_STACK_OVERFLOW ||
      code == EXCEPTION_ILLEGAL_INSTRUCTION ||
      code == EXCEPTION_PRIV_INSTRUCTION) {
    FILE *f = fopen("dc3dd_crash.log", "a");
    if (f) {
      fprintf(f, "CRASH: code=0x%08X EIP=0x%08X", code, ep->ContextRecord->Eip);
      if (code == EXCEPTION_ACCESS_VIOLATION)
        fprintf(f, " addr=0x%08X (%s)",
                (DWORD)ep->ExceptionRecord->ExceptionInformation[1],
                ep->ExceptionRecord->ExceptionInformation[0] ? "write"
                                                             : "read");
      fprintf(f, "\n  EAX=%08X EBX=%08X ECX=%08X EDX=%08X\n",
              ep->ContextRecord->Eax, ep->ContextRecord->Ebx,
              ep->ContextRecord->Ecx, ep->ContextRecord->Edx);
      fprintf(f, "  ESP=%08X EBP=%08X ESI=%08X EDI=%08X\n",
              ep->ContextRecord->Esp, ep->ContextRecord->Ebp,
              ep->ContextRecord->Esi, ep->ContextRecord->Edi);
      fclose(f);
    }
  }
  return EXCEPTION_CONTINUE_SEARCH;
}

// ============================================================================
// id_Data -> AdvData Hardlink Sync
// ============================================================================
// The CIRCUS engine's internal CRX image loader bypasses Win32 CreateFileA for
// some rendering codepaths (e.g. Scenario Mode navigation). To ensure translated
// graphics always load, we create hardlinks (or copies) from id_Data into the
// AdvData directories at startup, making files appear where the engine expects.

static void EnsureDirectoryExists(const char* path) {
  char tmp[MAX_PATH];
  strcpy_s(tmp, path);
  for (char* p = tmp; *p; p++) {
    if (*p == '\\' || *p == '/') {
      char saved = *p;
      *p = '\0';
      CreateDirectoryA(tmp, NULL);
      *p = saved;
    }
  }
  CreateDirectoryA(tmp, NULL);
}

static int SyncDirectoryRecursive(const char* srcDir, const char* srcRoot,
                                   const char* dstRoot, const char* cacheRoot,
                                   FILE* manifest) {
  int count = 0;
  char searchPath[MAX_PATH];
  sprintf_s(searchPath, "%s\\*", srcDir);

  WIN32_FIND_DATAA fd;
  HANDLE hFind = FindFirstFileA(searchPath, &fd);
  if (hFind == INVALID_HANDLE_VALUE) return 0;

  size_t srcRootLen = strlen(srcRoot);

  do {
    if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) continue;
    
    // CRITICAL: Ignore cache and manifest files to prevent infinite loop of copying backups!
    if (_stricmp(fd.cFileName, ".original_cache") == 0 ||
        _stricmp(fd.cFileName, ".sync_manifest") == 0) {
      continue;
    }

    char fullSrc[MAX_PATH];
    sprintf_s(fullSrc, "%s\\%s", srcDir, fd.cFileName);

    if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
      count += SyncDirectoryRecursive(fullSrc, srcRoot, dstRoot, cacheRoot, manifest);
    } else {
      // Only sync graphic/animation files
      const char* ext = strrchr(fd.cFileName, '.');
      if (!ext) continue;
      if (_stricmp(ext, ".crx") != 0 && _stricmp(ext, ".grp") != 0 &&
          _stricmp(ext, ".crm") != 0 && _stricmp(ext, ".pck") != 0) continue;

      // Build destination path: dstRoot + relative path after srcRoot
      const char* relPath = fullSrc + srcRootLen;
      char fullDst[MAX_PATH];
      sprintf_s(fullDst, "%s%s", dstRoot, relPath);

      // Ensure target directory exists
      char dstDir[MAX_PATH];
      strcpy_s(dstDir, fullDst);
      char* lastSep = strrchr(dstDir, '\\');
      if (lastSep) {
        *lastSep = '\0';
        EnsureDirectoryExists(dstDir);
      }

      // Back up the original AdvData file to cache before overwriting
      DWORD dstAttr = GetFileAttributesA(fullDst);
      if (dstAttr != INVALID_FILE_ATTRIBUTES) {
        char cachePath[MAX_PATH];
        sprintf_s(cachePath, "%s%s", cacheRoot, relPath);

        // Ensure cache directory exists
        char cacheDir[MAX_PATH];
        strcpy_s(cacheDir, cachePath);
        char* cacheSep = strrchr(cacheDir, '\\');
        if (cacheSep) {
          *cacheSep = '\0';
          EnsureDirectoryExists(cacheDir);
        }

        // Only back up if cache doesn't already have this file
        DWORD cacheAttr = GetFileAttributesA(cachePath);
        if (cacheAttr == INVALID_FILE_ATTRIBUTES) {
          MoveFileA(fullDst, cachePath);
        } else {
          DeleteFileA(fullDst);
        }
      }

      // Create hardlink (fall back to copy)
      if (CreateHardLinkA(fullDst, fullSrc, NULL) ||
          CopyFileA(fullSrc, fullDst, FALSE)) {
        count++;
        // Write to manifest
        if (manifest) {
          fprintf(manifest, "%s\n", fullDst);
        }
      }
    }
  } while (FindNextFileA(hFind, &fd));

  FindClose(hFind);
  return count;
}

static void SyncGraphicsFromDir(const char* srcRelDir, const char* manifestFile, const char* cacheRelDir) {
  char srcAbs[MAX_PATH], dstAbs[MAX_PATH], cacheAbs[MAX_PATH];
  GetFullPathNameA(srcRelDir, MAX_PATH, srcAbs, NULL);
  GetFullPathNameA(".\\AdvData", MAX_PATH, dstAbs, NULL);
  GetFullPathNameA(cacheRelDir, MAX_PATH, cacheAbs, NULL);

  DWORD attr = GetFileAttributesA(srcAbs);
  if (attr == INVALID_FILE_ATTRIBUTES || !(attr & FILE_ATTRIBUTE_DIRECTORY)) return;

  FILE* manifest = fopen(manifestFile, "w");
  int count = SyncDirectoryRecursive(srcAbs, srcAbs, dstAbs, cacheAbs, manifest);
  if (manifest) fclose(manifest);

  char msg[256];
  sprintf_s(msg, "DC3DDPatch: Synced %d graphic files from %s to AdvData\n", count, srcRelDir);
  OutputDebugStringA(msg);
}

static void CleanupSyncedFrom(const char* manifestFile, const char* cacheRelDir) {
  FILE* manifest = fopen(manifestFile, "r");
  if (!manifest) return;

  char cacheAbs[MAX_PATH], dstAbs[MAX_PATH];
  GetFullPathNameA(cacheRelDir, MAX_PATH, cacheAbs, NULL);
  GetFullPathNameA(".\\AdvData", MAX_PATH, dstAbs, NULL);

  size_t dstRootLen = strlen(dstAbs);

  int count = 0;
  char line[MAX_PATH];
  while (fgets(line, MAX_PATH, manifest)) {
    size_t len = strlen(line);
    while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'))
      line[--len] = '\0';
    if (len == 0) continue;

    DeleteFileA(line);

    if (len > dstRootLen) {
      const char* relPath = line + dstRootLen;
      char cachePath[MAX_PATH];
      sprintf_s(cachePath, "%s%s", cacheAbs, relPath);

      DWORD cacheAttr = GetFileAttributesA(cachePath);
      if (cacheAttr != INVALID_FILE_ATTRIBUTES) {
        MoveFileA(cachePath, line);
      }
    }
    count++;
  }
  fclose(manifest);
  DeleteFileA(manifestFile);

  char msg[256];
  sprintf_s(msg, "DC3DDPatch: Cleaned up %d synced files, restored originals from cache\n", count);
  OutputDebugStringA(msg);
}

// ============================================================================
// Initialization
// ============================================================================

// ============================================================================
// NOTE: PatchCharacterNameTable REMOVED — address 0x47D400 is NOT a character
// name table (it contains unrelated data in .rdata). Character name → icon
// mapping is now handled in Hook_CheckIcon via direct Latin name matching.
// ============================================================================


static bool PatchIAT(HMODULE hModule, const char *dllName, PROC oldFunc,
                     PROC newFunc);


static void SafeAttach(PVOID* ppPointer, PVOID pDetour, const char* name) {
    TraceLog("Attaching %s...", name);
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    LONG attachErr = DetourAttach(ppPointer, pDetour);
    LONG commitErr = DetourTransactionCommit();
    if (attachErr != 0 || commitErr != 0) {
        TraceLog("Failed to hook %s: attachErr=%ld commitErr=%ld", name, attachErr, commitErr);
    } else {
        TraceLog("Successfully hooked %s", name);
    }
}

static void InitPatch() {
  TraceLog("InitPatch begin");
  // Install crash logger first so we can diagnose any crashes
  AddVectoredExceptionHandler(1, CrashLogger);

  g_fontManager.Init();
  Real_CheckIcon = reinterpret_cast<int(__cdecl *)(const char *)>(
      g_fontManager.GetCheckIconAddress());
  Real_CheckIconConfig = reinterpret_cast<int(__cdecl *)(int, int)>(
      g_fontManager.GetCheckIconConfigAddress());
  Real_BacklogIconHandler = reinterpret_cast<int(__cdecl *)(const char *)>(
      g_fontManager.GetBacklogIconHandlerAddress());
  g_attachedCheckIcon = false;
  g_attachedCheckIconConfig = false;
  g_attachedBacklogIconHandler = false;

  // Read language setting
  int lang = g_fontManager.GetLanguage();

  // Handle id_Data (Indonesian) sync/cleanup
  if (lang == 1) {
    SyncGraphicsFromDir(".\\id_Data", ".\\id_Data\\.sync_manifest", ".\\id_Data\\.original_cache");
  } else {
    CleanupSyncedFrom(".\\id_Data\\.sync_manifest", ".\\id_Data\\.original_cache");
  }

  // Handle eng_data (English) sync/cleanup
  if (lang == 2) {
    SyncGraphicsFromDir(".\\eng_data", ".\\eng_data\\.sync_manifest", ".\\eng_data\\.original_cache");
  } else {
    CleanupSyncedFrom(".\\eng_data\\.sync_manifest", ".\\eng_data\\.original_cache");
  }

  // Patch Japanese strings in the EXE image with Indonesian translations
  PatchStringsInMemory();

  // DVD verification bypass - patch JE at 0x004100D1 to always return success
  // PatchDvdCheck();

  // Load saved backlog setting
  // Load saved backlog setting - FORCE ENABLED for testing
  // g_enableBacklogAllIcon = (GetFileAttributesA(EnableBacklogAllIconFile) !=
  // INVALID_FILE_ATTRIBUTES);
  // Removed g_enableBacklogAllIcon as it's now internal to FontManager


  // Set Japanese codepage (Shift-JIS) as early as possible.
  // This fixes encoding on Wine/Proton where GetACP() returns the wrong
  // value. LCID/LangID hooks are intentionally NOT included - they crash the
  // backlog.
  // NOTE: Real_ExtTextOutA/W, Real_TextOutA/W, Real_GetGlyphOutlineA/W, etc.
  // are initialized via IAT binding at declaration (= FunctionName).
  // Do NOT re-assign with GetProcAddress here — that would point Detours to
  // gdi32.dll's raw export instead of the IAT entry the game actually calls.

  // removed bulk tx begin
  // SafeAttach disabled for IAT -> Real_GetACP
  // SafeAttach disabled for IAT -> Real_GetOEMCP
  LONG detour1 = 0;
  TraceLog("InitPatch detour stage1 commit=%ld", detour1);

  // removed bulk tx begin
  // SafeAttach disabled for IAT -> Real_GetGlyphOutlineW
  // SafeAttach disabled for IAT -> Real_GetGlyphOutlineA
  // SafeAttach disabled for IAT -> Real_CreateFileA
  // SafeAttach disabled for IAT -> Real_CreateFileW
  // SafeAttach disabled for IAT -> Real_GetTextExtentPoint32A
  // SafeAttach disabled for IAT -> Real_GetTextExtentPoint32W
  // SafeAttach disabled for IAT -> Real_GetTextExtentExPointA
  // SafeAttach disabled for IAT -> Real_GetTextExtentExPointW
  // SafeAttach disabled for IAT -> Real_GetCharWidth32A
  // SafeAttach disabled for IAT -> Real_GetCharWidth32W
  // SafeAttach disabled for IAT -> Real_DrawTextA
  // SafeAttach disabled for IAT -> Real_DrawTextW
  // SafeAttach disabled for IAT -> Real_DrawTextExA
  // SafeAttach disabled for IAT -> Real_DrawTextExW
  // Removed MultiByteToWideChar hook - crashes with Locale Emulator
  // SafeAttach(&(PVOID &)Real_MultiByteToWideChar, (PVOID)Hook_MultiByteToWideChar, "Real_MultiByteToWideChar");
  // DVD/KEY verification bypass — LOGGING MODE (pass-through, no blocking)
  // SafeAttach disabled for IAT -> Real_MessageBoxA
  // SafeAttach disabled for IAT -> Real_MessageBoxW
  // SafeAttach disabled for IAT -> Real_MessageBoxExA
  // SafeAttach disabled for IAT -> Real_MessageBoxExW
  // SafeAttach disabled for IAT -> Real_MessageBoxIndirectA
  // SafeAttach disabled for IAT -> Real_MessageBoxIndirectW
  // SafeAttach disabled for IAT -> Real_DialogBoxParamA
  // Indonesian UI translation - menu hooks
  // SafeAttach disabled for IAT -> Real_AppendMenuA
  // SafeAttach disabled for IAT -> Real_InsertMenuA
  // SafeAttach disabled for IAT -> Real_ModifyMenuA
  // SafeAttach disabled for IAT -> Real_AppendMenuW
  // SafeAttach disabled for IAT -> Real_InsertMenuW
  // SafeAttach disabled for IAT -> Real_ModifyMenuW
  // SafeAttach disabled for IAT -> Real_TrackPopupMenu
  // SafeAttach disabled for IAT -> Real_TrackPopupMenuEx
  // Backlog font size reduction - hook the backlog function entry/exit.
  // These are version-dependent offsets, so attach only when executable.
  if (IsExecutableAddress(reinterpret_cast<void *>(Real_CheckIcon))) {
    SafeAttach(&(PVOID &)Real_CheckIcon, (PVOID)Hook_CheckIcon, "Real_CheckIcon");
    g_attachedCheckIcon = true;
  }
  if (IsExecutableAddress(reinterpret_cast<void *>(Real_CheckIconConfig))) {
    SafeAttach(&(PVOID &)Real_CheckIconConfig, (PVOID)Hook_CheckIconConfig, "Real_CheckIconConfig");
    g_attachedCheckIconConfig = true;
  }
  if (IsExecutableAddress(reinterpret_cast<void *>(Real_BacklogIconHandler))) {
    // SafeAttach(&(PVOID &)Real_BacklogIconHandler, (PVOID)Hook_BacklogIconHandler, "Real_BacklogIconHandler");
    // g_attachedBacklogIconHandler = true;
  }
  // Backlog line spacing and text position
  // SafeAttach disabled for IAT -> Real_GetTextMetricsA
  // SafeAttach disabled for IAT -> Real_CreateWindowExA
  // SafeAttach disabled for IAT -> Real_CreateWindowExW
  // SafeAttach disabled for IAT -> Real_SetWindowTextA
  // SafeAttach disabled for IAT -> Real_SetWindowTextW
  // Detours hooks for CreateFont APIs conflict with Locale Emulator's inline hooks.
  // We use IAT hooking to intercept the calls BEFORE they reach LE's inline hooks.
  // SafeAttach(&(PVOID &)Real_CreateFontIndirectA, (PVOID)Hook_CreateFontIndirectA, "Real_CreateFontIndirectA");
  // SafeAttach(&(PVOID &)Real_CreateFontIndirectW, (PVOID)Hook_CreateFontIndirectW, "Real_CreateFontIndirectW");
  // SafeAttach(&(PVOID &)Real_CreateFontA, (PVOID)Hook_CreateFontA, "Real_CreateFontA");
  // SafeAttach(&(PVOID &)Real_CreateFontW, (PVOID)Hook_CreateFontW, "Real_CreateFontW");

  // SafeAttach(&(PVOID &)Real_ExtTextOutA, reinterpret_cast<PVOID>(Hook_ExtTextOutA), "Real_ExtTextOutA");
  // SafeAttach(&(PVOID &)Real_ExtTextOutW, reinterpret_cast<PVOID>(Hook_ExtTextOutW), "Real_ExtTextOutW");
  // SafeAttach(&(PVOID &)Real_TextOutA, reinterpret_cast<PVOID>(Hook_TextOutA), "Real_TextOutA");
  // SafeAttach(&(PVOID &)Real_TextOutW, reinterpret_cast<PVOID>(Hook_TextOutW), "Real_TextOutW");
  
  // NOTE: Hook_TitleLookup removed — 0x406120 is not a function entry point
  // (first byte is 0xE8=CALL), Detours corrupted the code stream.
  // Scenario titles now handled via patched sce_*.mes files in id_Data/.

  LONG detour2 = 0;
  TraceLog("InitPatch detour stage2 commit=%ld", detour2);

  // Fallback IAT hooks for critical UI/menu path in case detours fail on target build.
  HMODULE hMain = GetModuleHandleA(NULL);
  PatchIAT(hMain, "GDI32.dll", (PROC)CreateFontA, (PROC)Hook_CreateFontA);
  PatchIAT(hMain, "GDI32.dll", (PROC)CreateFontW, (PROC)Hook_CreateFontW);

  // IAT hooks to bypass Locale Emulator inline hook conflicts
  PatchIAT(hMain, "GDI32.dll", (PROC)CreateFontIndirectA, (PROC)Hook_CreateFontIndirectA);
  PatchIAT(hMain, "GDI32.dll", (PROC)CreateFontIndirectW, (PROC)Hook_CreateFontIndirectW);
  PatchIAT(hMain, "KERNEL32.dll", (PROC)GetACP, (PROC)Hook_GetACP);
  PatchIAT(hMain, "KERNEL32.dll", (PROC)GetOEMCP, (PROC)Hook_GetOEMCP);
  PatchIAT(hMain, "GDI32.dll", (PROC)GetGlyphOutlineW, (PROC)Hook_GetGlyphOutlineW);
  PatchIAT(hMain, "GDI32.dll", (PROC)GetGlyphOutlineA, (PROC)Hook_GetGlyphOutlineA);
  PatchIAT(hMain, "KERNEL32.dll", (PROC)CreateFileA, (PROC)Hook_CreateFileA);
  PatchIAT(hMain, "KERNEL32.dll", (PROC)CreateFileW, (PROC)Hook_CreateFileW);
  PatchIAT(hMain, "GDI32.dll", (PROC)GetTextExtentPoint32A, (PROC)Hook_GetTextExtentPoint32A);
  PatchIAT(hMain, "GDI32.dll", (PROC)GetTextExtentPoint32W, (PROC)Hook_GetTextExtentPoint32W);
  PatchIAT(hMain, "GDI32.dll", (PROC)GetTextExtentExPointA, (PROC)Hook_GetTextExtentExPointA);
  PatchIAT(hMain, "GDI32.dll", (PROC)GetTextExtentExPointW, (PROC)Hook_GetTextExtentExPointW);
  PatchIAT(hMain, "GDI32.dll", (PROC)GetCharWidth32A, (PROC)Hook_GetCharWidth32A);
  PatchIAT(hMain, "GDI32.dll", (PROC)GetCharWidth32W, (PROC)Hook_GetCharWidth32W);
  PatchIAT(hMain, "USER32.dll", (PROC)DrawTextA, (PROC)Hook_DrawTextA);
  PatchIAT(hMain, "USER32.dll", (PROC)DrawTextW, (PROC)Hook_DrawTextW);
  PatchIAT(hMain, "USER32.dll", (PROC)DrawTextExA, (PROC)Hook_DrawTextExA);
  PatchIAT(hMain, "USER32.dll", (PROC)DrawTextExW, (PROC)Hook_DrawTextExW);
  PatchIAT(hMain, "USER32.dll", (PROC)MessageBoxA, (PROC)Hook_MessageBoxA);
  PatchIAT(hMain, "USER32.dll", (PROC)MessageBoxW, (PROC)Hook_MessageBoxW);
  PatchIAT(hMain, "USER32.dll", (PROC)MessageBoxExA, (PROC)Hook_MessageBoxExA);
  PatchIAT(hMain, "USER32.dll", (PROC)MessageBoxExW, (PROC)Hook_MessageBoxExW);
  PatchIAT(hMain, "USER32.dll", (PROC)MessageBoxIndirectA, (PROC)Hook_MessageBoxIndirectA);
  PatchIAT(hMain, "USER32.dll", (PROC)MessageBoxIndirectW, (PROC)Hook_MessageBoxIndirectW);
  PatchIAT(hMain, "USER32.dll", (PROC)DialogBoxParamA, (PROC)Hook_DialogBoxParamA);
  PatchIAT(hMain, "USER32.dll", (PROC)AppendMenuA, (PROC)Hook_AppendMenuA);
  PatchIAT(hMain, "USER32.dll", (PROC)InsertMenuA, (PROC)Hook_InsertMenuA);
  PatchIAT(hMain, "USER32.dll", (PROC)ModifyMenuA, (PROC)Hook_ModifyMenuA);
  PatchIAT(hMain, "USER32.dll", (PROC)AppendMenuW, (PROC)Hook_AppendMenuW);
  PatchIAT(hMain, "USER32.dll", (PROC)InsertMenuW, (PROC)Hook_InsertMenuW);
  PatchIAT(hMain, "USER32.dll", (PROC)ModifyMenuW, (PROC)Hook_ModifyMenuW);
  PatchIAT(hMain, "USER32.dll", (PROC)TrackPopupMenu, (PROC)Hook_TrackPopupMenu);
  PatchIAT(hMain, "USER32.dll", (PROC)TrackPopupMenuEx, (PROC)Hook_TrackPopupMenuEx);
  PatchIAT(hMain, "GDI32.dll", (PROC)GetTextMetricsA, (PROC)Hook_GetTextMetricsA);
  PatchIAT(hMain, "USER32.dll", (PROC)CreateWindowExA, (PROC)Hook_CreateWindowExA);
  PatchIAT(hMain, "USER32.dll", (PROC)CreateWindowExW, (PROC)Hook_CreateWindowExW);
  PatchIAT(hMain, "USER32.dll", (PROC)SetWindowTextA, (PROC)Hook_SetWindowTextA);
  PatchIAT(hMain, "USER32.dll", (PROC)SetWindowTextW, (PROC)Hook_SetWindowTextW);
  PatchIAT(hMain, "GDI32.dll", (PROC)ExtTextOutA, (PROC)Hook_ExtTextOutA);
  PatchIAT(hMain, "GDI32.dll", (PROC)ExtTextOutW, (PROC)Hook_ExtTextOutW);
  PatchIAT(hMain, "GDI32.dll", (PROC)TextOutA, (PROC)Hook_TextOutA);
  PatchIAT(hMain, "GDI32.dll", (PROC)TextOutW, (PROC)Hook_TextOutW);

  TraceLog("InitPatch end");
}

static void CleanupPatch() {
  // Note: JmpWrite hook at CheckIcon else branch does not need cleanup
  // (game is exiting anyway, and restoring displaced bytes is complex)

  // Detach codepage hooks
  // removed bulk tx begin
  DetourDetach(&(PVOID &)Real_GetACP, Hook_GetACP);
  DetourDetach(&(PVOID &)Real_GetOEMCP, Hook_GetOEMCP);
  DetourTransactionCommit();

  // removed bulk tx begin
  DetourDetach(&(PVOID &)Real_GetGlyphOutlineW, Hook_GetGlyphOutlineW);
  DetourDetach(&(PVOID &)Real_GetGlyphOutlineA, Hook_GetGlyphOutlineA);
  DetourDetach(&(PVOID &)Real_CreateFileA, Hook_CreateFileA);
  DetourDetach(&(PVOID &)Real_CreateFileW, Hook_CreateFileW);
  DetourDetach(&(PVOID &)Real_CreateWindowExA, Hook_CreateWindowExA);
  DetourDetach(&(PVOID &)Real_GetTextExtentPoint32A,
               Hook_GetTextExtentPoint32A);
  DetourDetach(&(PVOID &)Real_GetTextExtentPoint32W,
               Hook_GetTextExtentPoint32W);
  DetourDetach(&(PVOID &)Real_GetTextExtentExPointA,
               Hook_GetTextExtentExPointA);
  DetourDetach(&(PVOID &)Real_GetTextExtentExPointW,
               Hook_GetTextExtentExPointW);
  DetourDetach(&(PVOID &)GetCharWidth32A, Hook_GetCharWidth32A);
  DetourDetach(&(PVOID &)GetCharWidth32W, Hook_GetCharWidth32W);
  DetourDetach(&(PVOID &)Real_MessageBoxA, Hook_MessageBoxA);
  DetourDetach(&(PVOID &)Real_MessageBoxW, Hook_MessageBoxW);
  DetourDetach(&(PVOID &)Real_MessageBoxExA, Hook_MessageBoxExA);
  DetourDetach(&(PVOID &)Real_MessageBoxExW, Hook_MessageBoxExW);
  DetourDetach(&(PVOID &)Real_MessageBoxIndirectA, Hook_MessageBoxIndirectA);
  DetourDetach(&(PVOID &)Real_MessageBoxIndirectW, Hook_MessageBoxIndirectW);
  DetourDetach(&(PVOID &)Real_DialogBoxParamA, Hook_DialogBoxParamA);
  DetourDetach(&(PVOID &)Real_AppendMenuA, Hook_AppendMenuA);
  DetourDetach(&(PVOID &)Real_InsertMenuA, Hook_InsertMenuA);
  DetourDetach(&(PVOID &)Real_ModifyMenuA, Hook_ModifyMenuA);
  DetourDetach(&(PVOID &)Real_AppendMenuW, Hook_AppendMenuW);
  DetourDetach(&(PVOID &)Real_InsertMenuW, Hook_InsertMenuW);
  DetourDetach(&(PVOID &)Real_ModifyMenuW, Hook_ModifyMenuW);
  DetourDetach(&(PVOID &)Real_TrackPopupMenu, Hook_TrackPopupMenu);
  DetourDetach(&(PVOID &)Real_TrackPopupMenuEx, Hook_TrackPopupMenuEx);
  if (g_attachedCheckIcon) {
    DetourDetach(&(PVOID &)Real_CheckIcon, Hook_CheckIcon);
  }
  if (g_attachedCheckIconConfig) {
    DetourDetach(&(PVOID &)Real_CheckIconConfig, Hook_CheckIconConfig);
  }
  if (g_attachedBacklogIconHandler) {
    DetourDetach(&(PVOID &)Real_BacklogIconHandler, Hook_BacklogIconHandler);
  }
  // DetourDetach(&(PVOID &)Real_BacklogFunc, Hook_BacklogFunc);
  DetourDetach(&(PVOID &)Real_GetTextMetricsA, Hook_GetTextMetricsA);
  DetourDetach(&(PVOID &)Real_ExtTextOutA, reinterpret_cast<PVOID>(Hook_ExtTextOutA));
  DetourDetach(&(PVOID &)Real_ExtTextOutW, reinterpret_cast<PVOID>(Hook_ExtTextOutW));
  DetourDetach(&(PVOID &)Real_TextOutA, reinterpret_cast<PVOID>(Hook_TextOutA));
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call,
                      LPVOID lpReserved) {
  if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
    TraceLog("DllMain PROCESS_ATTACH module=%p", hModule);
    DisableThreadLibraryCalls(hModule);
    InitPatch();
  } else if (ul_reason_for_call == DLL_PROCESS_DETACH) {
    TraceLog("DllMain PROCESS_DETACH module=%p", hModule);
    CleanupPatch();
  }
  return TRUE;
}

