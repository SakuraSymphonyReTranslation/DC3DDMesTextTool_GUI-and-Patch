import codecs

with open(r"H:\Games\DC3PP\DC3PP.EXE", "rb") as f:
    data = f.read()

# Extract scenario titles from known offset range
titles = []
i = 0x05D400
end = 0x05F6C0

while i < end:
    if data[i] >= 0x81 and (data[i] <= 0x9F or (data[i] >= 0xE0 and data[i] <= 0xEF)):
        start = i
        while i < end and data[i] != 0:
            if (data[i] >= 0x81 and data[i] <= 0x9F) or (data[i] >= 0xE0 and data[i] <= 0xEF):
                i += 2
            elif data[i] >= 0x20 and data[i] < 0x7F:
                i += 1
            else:
                break
        if data[i] == 0 and (i - start) >= 4:
            try:
                s = data[start:i].decode("cp932")
                has_jp = any(ord(c) > 0x3000 for c in s)
                if has_jp:
                    titles.append(s)
            except:
                pass
    i += 1

# Write to INI format
with open(r"H:\Games\DC3PP\DC3PPMesTextTool_GUI - Copy berantakan\DC4Patch a\scenario_titles.ini", "w", encoding="utf-8-sig") as f:
    f.write("[ScenarioTitles]\n")
    f.write("; Format: Japanese Title=Indonesian Translation\n")
    f.write("; Edit the right side of = to add your translation\n")
    f.write("; Lines starting with ; are comments\n\n")
    seen = set()
    count = 0
    for t in titles:
        if t not in seen:
            seen.add(t)
            f.write(f"{t}={t}\n")
            count += 1

print(f"Extracted {count} unique scenario titles")
print("Saved to scenario_titles.ini")
