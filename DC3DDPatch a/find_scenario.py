import codecs

with open(r"H:\Games\DC3PP\DC3PP.EXE", "rb") as f:
    data = f.read()

tests = ["体育祭、開幕！", "決戦の時", "プロローグ", "桜の季節", "初めまして", "約束"]
for t in tests:
    encoded = t.encode("cp932")
    pos = data.find(encoded)
    if pos >= 0:
        null_pos = data.find(b"\x00", pos)
        full = data[pos:null_pos]
        decoded = full.decode("cp932", errors="replace")
        print(f"Found [{t}] at 0x{pos:X}, full: [{decoded}] ({len(full)} bytes)")
    else:
        print(f"NOT found: {t}")

# Also dump all SJIS strings from .rdata that look like scenario titles
# Find strings between 4-40 bytes containing kanji
print("\n--- Scanning for potential scenario title strings ---")
found = []
i = 0
while i < len(data):
    if data[i] >= 0x81 and data[i] <= 0x9F or data[i] >= 0xE0 and data[i] <= 0xEF:
        # Potential SJIS lead byte
        start = i
        while i < len(data) and data[i] != 0:
            if (data[i] >= 0x81 and data[i] <= 0x9F) or (data[i] >= 0xE0 and data[i] <= 0xEF):
                i += 2  # SJIS double-byte
            elif data[i] >= 0x20 and data[i] < 0x7F:
                i += 1  # ASCII
            else:
                break
        if data[i] == 0 and (i - start) >= 6 and (i - start) <= 60:
            try:
                s = data[start:i].decode("cp932")
                # Filter: must contain at least one kanji/hiragana
                has_jp = any(ord(c) > 0x3000 for c in s)
                if has_jp and not s.startswith("\\") and not s.startswith("/"):
                    found.append((start, s))
            except:
                pass
    i += 1

# Print unique strings sorted by offset
seen = set()
for offset, s in found:
    if s not in seen and len(s) >= 3:
        seen.add(s)
        # Only print strings that look like titles (short, no paths)
        if len(s) <= 30 and "\\" not in s and "/" not in s and "." not in s:
            print(f"  0x{offset:06X}: {s}")
