with open("e:/Games/DC3DD/DC3DDMesTextTool_GUI and Patch/DC3DDPatch a/src/dc4patch.cpp", "r", encoding="utf-8") as f:
    content = f.read()

content = content.replace("static HFONT WINAPI Hook_CreateFontIndirectA(LOGFONTA *lplf) {\n    if (lplf && !g_inChooseFont) {", "static HFONT WINAPI Hook_CreateFontIndirectA(LOGFONTA *lplf) {\n    if (lplf && !g_inChooseFont) {\n        TraceLog(\"Hook_CreateFontIndirectA: height=%d\", lplf->lfHeight);")
content = content.replace("static HFONT WINAPI Hook_CreateFontA(int cHeight, int cWidth, int cEscapement, int cOrientation, int cWeight, DWORD bItalic, DWORD bUnderline, DWORD bStrikeOut, DWORD iCharSet, DWORD iOutPrecision, DWORD iClipPrecision, DWORD iQuality, DWORD iPitchAndFamily, LPCSTR pszFaceName) {\n    if (!g_inChooseFont) {", "static HFONT WINAPI Hook_CreateFontA(int cHeight, int cWidth, int cEscapement, int cOrientation, int cWeight, DWORD bItalic, DWORD bUnderline, DWORD bStrikeOut, DWORD iCharSet, DWORD iOutPrecision, DWORD iClipPrecision, DWORD iQuality, DWORD iPitchAndFamily, LPCSTR pszFaceName) {\n    if (!g_inChooseFont) {\n        TraceLog(\"Hook_CreateFontA: height=%d\", cHeight);")

with open("e:/Games/DC3DD/DC3DDMesTextTool_GUI and Patch/DC3DDPatch a/src/dc4patch.cpp", "w", encoding="utf-8") as f:
    f.write(content)
print("Trace logic added.")
