"""
Properly patch DC4 sce_*.mes files by replacing ◆title◆ patterns
with Indonesian translations, rebuilding section offsets.

The MES format:
- Header: [numSections:4][numEntries:4][sectionOffset0:4][sectionOffset1:4]...
- Sections: contiguous byte ranges
- Strings like ◆title◆ are opcode arguments, null-terminator is implicit via 2nd ◆

When we insert/remove bytes, all section offsets after the insertion point
must be adjusted.
"""
import os, sys, re, struct, glob
sys.stdout.reconfigure(encoding='utf-8')

# Load translations from dc4patch.cpp
cpp_file = r'D:\vns\DC4\DC4Patch\src\dc4patch.cpp'
with open(cpp_file, 'r', encoding='utf-8') as f:
    cpp_text = f.read()

translations = {}  # sjis_bytes -> indonesian_string
pattern = r'\{"((?:\\x[0-9a-fA-F]{2})+(?:"\s*"(?:\\x[0-9a-fA-F]{2})+)*)",\s*\n?\s*"([^"]+)"\}'
for m in re.finditer(pattern, cpp_text):
    sjis_hex = m.group(1).replace('" "', '')
    indo = m.group(2)
    sjis_bytes = bytes(int(h, 16) for h in re.findall(r'\\x([0-9a-fA-F]{2})', sjis_hex))
    translations[sjis_bytes] = indo

print(f"Loaded {len(translations)} translations")

diamond_marker = bytes([0x81, 0x9F])  # ◆ in SJIS

def patch_mes_file(input_path, output_path):
    """
    Patch a MES file: replace ◆jp_title◆ with ◆id_title◆.
    Rebuilds the section offset table in the header since string lengths change.
    """
    with open(input_path, 'rb') as f:
        data = bytearray(f.read())
    
    basename = os.path.basename(input_path)
    
    # Parse header: [numSections:4][numEntries:4][offsets...]
    num_sections = struct.unpack_from('<I', data, 0)[0]
    num_entries  = struct.unpack_from('<I', data, 4)[0]
    
    # Header size = 8 + num_sections * 4
    header_size = 8 + num_sections * 4
    
    # Read original section offsets
    original_offsets = []
    for i in range(num_sections):
        off = struct.unpack_from('<I', data, 8 + i*4)[0]
        original_offsets.append(off)
    
    # Find and replace all ◆title◆ pairs
    total_translated = 0
    total_skipped = 0
    size_delta = 0  # track cumulative size change
    
    # We'll build the patched data in a list of bytearray segments
    # to handle variable-length replacements
    patched = bytearray()
    pos = 0
    replacement_points = []  # (original_offset, delta_before_this_point)
    
    # Pass 1: find all ◆title◆ pairs and their positions
    replacements = []  # list of (start, end, new_content)
    search_pos = 0
    while True:
        open_pos = data.find(diamond_marker, search_pos)
        if open_pos < 0:
            break
        close_pos = data.find(diamond_marker, open_pos + 2)
        if close_pos < 0:
            break
        
        title_sjis = bytes(data[open_pos+2:close_pos])
        
        if title_sjis in translations:
            indo = translations[title_sjis]
            # Encode the translation
            try:
                # Try SJIS first
                indo_bytes = indo.encode('shift-jis', errors='strict')
            except:
                # Fallback: use only ASCII-safe portion
                indo_safe = ''.join(c for c in indo if ord(c) < 128)
                indo_bytes = indo_safe.encode('ascii', errors='replace')
            
            # New content: ◆ + translation + ◆
            new_content = diamond_marker + indo_bytes + diamond_marker
            old_content_len = 2 + len(title_sjis) + 2  # ◆ + title + ◆
            
            replacements.append((open_pos, open_pos + old_content_len, new_content))
            total_translated += 1
        else:
            total_skipped += 1
        
        search_pos = close_pos + 2
    
    if not replacements:
        return False, 0, 0
    
    # Build patched data by applying replacements
    result = bytearray()
    prev_end = 0
    accum_delta = 0  # cumulative byte delta for offset adjustment
    
    # We need to track where each replacement happens relative to original offsets
    # Build (original_pos, delta) pairs sorted by position
    deltas_by_pos = []  # (original_pos_after_replacement, cumulative_delta)
    
    for (start, end, new_content) in replacements:
        result.extend(data[prev_end:start])
        result.extend(new_content)
        old_len = end - start
        new_len = len(new_content)
        delta = new_len - old_len
        accum_delta += delta
        deltas_by_pos.append((end, accum_delta))
        prev_end = end
    
    result.extend(data[prev_end:])
    
    # Now update section offsets in the header
    # For each section offset, add the cumulative delta that has occurred before it
    final_accum_delta = 0
    new_offsets = []
    for i, orig_off in enumerate(original_offsets):
        # Find cumulative delta before this offset
        adj_delta = 0
        for (pos_after, delta) in deltas_by_pos:
            if pos_after <= orig_off:
                adj_delta = delta
            else:
                break
        new_offsets.append(orig_off + adj_delta)
    
    # Patch offsets in header
    for i, new_off in enumerate(new_offsets):
        struct.pack_into('<I', result, 8 + i*4, new_off)
    
    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    with open(output_path, 'wb') as f:
        f.write(result)
    
    return True, total_translated, total_skipped


mes_dir = r'D:\vns\DC4\AdvData\MES jp'
output_dir = r'D:\vns\DC4\id_Data'  # FLAT - ReplacePathA only keeps filename

sce_files = sorted(glob.glob(os.path.join(mes_dir, 'sce_*.mes')))
print(f"\nPatching {len(sce_files)} sce_*.mes files...")

total_all = 0
for filepath in sce_files:
    basename = os.path.basename(filepath)
    output_path = os.path.join(output_dir, basename)
    success, translated, skipped = patch_mes_file(filepath, output_path)
    
    if success:
        orig_size = os.path.getsize(filepath)
        new_size  = os.path.getsize(output_path)
        print(f"  {basename}: {translated} translated, {skipped} skipped | {orig_size} -> {new_size} bytes (+{new_size-orig_size})")
        total_all += translated
    else:
        print(f"  {basename}: no translations applied")

print(f"\nTotal translations: {total_all}")
