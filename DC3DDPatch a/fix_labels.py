"""
Fix MES label table after mes_json_importer.exe
The old importer does NOT recalculate labels when text changes length.

Key insight: Labels can point to MID-token positions inside string tokens.
For non-string tokens, the byte-level mapping is 1:1 (same length).
For string tokens, we interpolate: since text content changes but opcode byte
stays, we map (orig_token_start -> mod_token_start) and for positions after
the opcode byte, we compute proportional offsets within the string data.

Actually, a simpler approach: for tokens that don't change length (non-text),
positions inside map 1:1. For string tokens that change length, labels inside  
them need careful handling. But labels inside string tokens typically point to
byte positions that represent branch targets set by the compiler — these should
map to the SAME relative position within the token structure.

Looking at the C++ MesTextTool's import_text more carefully, it recalculates
labels by checking if a label points to (token.offset + first_token_length).
This means labels point to SPECIFIC bytecode positions, not arbitrary positions
within strings.

NEW APPROACH: Don't try to map mid-token offsets at all. Instead, build the
complete byte-for-byte mapping between original and modified bytecode by
iterating tokens in parallel and tracking cumulative offsets.
"""
import struct
import os
import sys
import glob

ENC_KEY = 0x20
OP_UINT8_2 = (0x00, 0x3A)
OP_UINT8_STR = (0x3B, 0x47)
OP_STRING = (0x48, 0x68)
OP_ENC_STR = (0x69, 0x6D)
OP_UINT16_4 = (0x6E, 0xFF)


def get_string_length(data, offset):
    length = 0
    while offset + length < len(data) and data[offset + length] != 0:
        length += 1
    return length


def tokenize(data, bytecode_offset, version_len=2):
    """Tokenize, returning list of (absolute_pos, length, opcode, is_text)"""
    pos = bytecode_offset + version_len
    tokens = []
    while pos < len(data):
        opcode = data[pos]
        is_text = False
        if OP_UINT8_2[0] <= opcode <= OP_UINT8_2[1]:
            tlen = 3
        elif OP_UINT8_STR[0] <= opcode <= OP_UINT8_STR[1]:
            s = get_string_length(data, pos + 2)
            tlen = 2 + s + 1
            is_text = True
        elif OP_STRING[0] <= opcode <= OP_STRING[1]:
            s = get_string_length(data, pos + 1)
            tlen = 1 + s + 1
            is_text = True
        elif OP_ENC_STR[0] <= opcode <= OP_ENC_STR[1]:
            s = get_string_length(data, pos + 1)
            tlen = 1 + s + 1
            is_text = True
        elif OP_UINT16_4[0] <= opcode <= OP_UINT16_4[1]:
            tlen = 9
        else:
            tlen = 1
        tokens.append((pos, tlen, opcode, is_text))
        pos += tlen
    return tokens


def fix_labels(orig_path, mod_path, output_path=None):
    """Fix the label table by building a complete byte-to-byte offset map."""
    with open(orig_path, 'rb') as f:
        orig = f.read()
    with open(mod_path, 'rb') as f:
        mod = bytearray(f.read())
    
    label_count = struct.unpack('<I', orig[0:4])[0]
    bytecode_offset = label_count * 4 + 4
    version_len = 2
    
    orig_tokens = tokenize(orig, bytecode_offset, version_len)
    mod_tokens = tokenize(mod, bytecode_offset, version_len)
    
    if len(orig_tokens) != len(mod_tokens):
        raise ValueError(f"Token count mismatch: {len(orig_tokens)} vs {len(mod_tokens)}")
    
    # Build cumulative offset delta map
    # For each byte position in the original, calculate how much the position
    # shifts in the modified file.
    # Non-text tokens: same length, delta unchanged
    # Text tokens: delta changes by (mod_len - orig_len)
    
    # We need to handle labels that point to ANY byte position, including mid-token.
    # Strategy: for each absolute byte position in [bytecode_offset, end), 
    # compute the corresponding position in the modified file.
    
    # Build a list of (orig_abs_start, orig_len, mod_abs_start, mod_len) for each token
    token_map = []
    for ot, mt in zip(orig_tokens, mod_tokens):
        token_map.append((ot[0], ot[1], mt[0], mt[1]))
    
    # Now fix labels
    fixed = 0
    for i in range(label_count):
        label_pos = 4 + i * 4
        old_label = struct.unpack('<I', orig[label_pos:label_pos+4])[0]
        
        is_masked = (old_label & 0x80000000) != 0
        raw_label = old_label & 0x7FFFFFFF
        
        # Check if it points into bytecode area
        bc_start = bytecode_offset + version_len
        if raw_label < bc_start or raw_label >= len(orig):
            # Label outside bytecode, check small values (relative labels)
            if raw_label < bytecode_offset:
                continue
            # Points into version token area, keep as-is
            continue
        
        # Find which token this label falls in
        new_raw = None
        for orig_abs, orig_len, mod_abs, mod_len in token_map:
            if orig_abs <= raw_label < orig_abs + orig_len:
                # Label is inside this token
                offset_within = raw_label - orig_abs
                
                if orig_len == mod_len:
                    # Same length token, map 1:1
                    new_raw = mod_abs + offset_within
                else:
                    # Text token with different length
                    # The opcode byte (first byte) maps 1:1
                    # For positions within the string data, we need to be careful.
                    # Labels typically point to positions that the game uses for
                    # branch targets. For string content that changed length,
                    # we can't do proportional mapping — we keep the same
                    # offset from the END of the token instead (since labels
                    # often point to the null terminator or near the end).
                    
                    if offset_within == 0:
                        # Points to opcode byte
                        new_raw = mod_abs
                    elif offset_within >= orig_len:
                        # Points past the end (shouldn't happen, but handle)
                        new_raw = mod_abs + mod_len
                    else:
                        # Mid-token: use same distance from token end
                        dist_from_end = orig_len - offset_within
                        if dist_from_end <= mod_len:
                            new_raw = mod_abs + mod_len - dist_from_end
                        else:
                            # Fallback: same offset from start (clamped)
                            new_raw = mod_abs + min(offset_within, mod_len - 1)
                break
        
        if new_raw is None:
            # Check if points to exactly the end of the last token
            last_orig = token_map[-1]
            end_pos = last_orig[0] + last_orig[1]
            if raw_label == end_pos:
                last_mod = token_map[-1]
                new_raw = last_mod[2] + last_mod[3]
            else:
                continue
        
        if is_masked:
            new_label = 0x80000000 | (new_raw & 0x7FFFFFFF)
        else:
            new_label = new_raw
        
        struct.pack_into('<I', mod, label_pos, new_label & 0xFFFFFFFF)
        if new_label != old_label:
            fixed += 1
    
    if output_path is None:
        output_path = mod_path
    
    with open(output_path, 'wb') as f:
        f.write(mod)
    
    return fixed, label_count


def main():
    if len(sys.argv) < 3:
        print("Usage: fix_labels.py <original_mes_dir> <modified_mes_dir> [output_dir]")
        sys.exit(1)
    
    orig_dir = sys.argv[1]
    mod_dir = sys.argv[2]
    out_dir = sys.argv[3] if len(sys.argv) > 3 else mod_dir
    
    if out_dir != mod_dir:
        os.makedirs(out_dir, exist_ok=True)
    
    mod_files = glob.glob(os.path.join(mod_dir, "*.mes"))
    
    total_fixed = 0
    total_files = 0
    errors = 0
    
    for mod_path in sorted(mod_files):
        fname = os.path.basename(mod_path)
        orig_path = os.path.join(orig_dir, fname)
        out_path = os.path.join(out_dir, fname)
        
        if not os.path.exists(orig_path):
            continue
        
        try:
            fixed, total = fix_labels(orig_path, mod_path, out_path)
            if fixed > 0:
                print(f"  {fname}: fixed {fixed}/{total} labels")
            total_fixed += fixed
            total_files += 1
        except Exception as e:
            print(f"  ERROR {fname}: {e}")
            errors += 1
    
    print(f"\nDone! Processed {total_files} files, fixed {total_fixed} labels total, {errors} errors")


if __name__ == "__main__":
    main()
