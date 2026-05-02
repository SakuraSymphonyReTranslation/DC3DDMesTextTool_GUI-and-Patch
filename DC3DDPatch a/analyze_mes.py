"""
Analyze actual DC3DD MES file to see which opcodes are used for dialogue text.
This tells us if narrative text is in EncStr (0x63-0x67) or String (0x58).
"""
import os, glob

# Find a MES file to analyze
mes_dir = r'E:\Games\DC3DD\Translated\MES'
files = glob.glob(os.path.join(mes_dir, '*.mes')) + glob.glob(os.path.join(mes_dir, '*.MES'))

if not files:
    print('No MES files found in', mes_dir)
    exit()

f_path = files[0]
print('Analyzing:', f_path)

with open(f_path, 'rb') as f:
    data = f.read()

# DC3DD: offset2 -> label_count*6+4 bytes for header
raw_count = int.from_bytes(data[0:4], 'little')
bytecode_offset = raw_count * 6 + 4

# EncStr range 0x63-0x67, String range 0x44-0x62, OutputOpcodes {0x58}
ENC_KEY = 0x20

# Parse tokens starting at bytecode_offset + 3 (version bytes)
pos = bytecode_offset + 3
print(f'Bytecode starts at 0x{bytecode_offset:X}, analysis starts at 0x{pos:X}')
print()

def read_string(data, offset):
    s = []
    while offset < len(data) and data[offset] != 0:
        s.append(data[offset])
        offset += 1
    return bytes(s)

def decrypt_string(data, offset, key=0x20):
    s = []
    while offset < len(data) and data[offset] != 0:
        s.append((data[offset] + key) & 0xFF)
        offset += 1
    return bytes(s)

count = 0
enc_texts = 0
str_texts = 0
shown = 0

while pos < len(data) and shown < 20:
    opcode = data[pos]
    
    if 0x44 <= opcode <= 0x62:  # String range (includes 0x58)
        raw = read_string(data, pos + 1)
        try:
            text = raw.decode('shift-jis', errors='replace')
        except:
            text = repr(raw[:30])
        if text.strip():
            str_texts += 1
            if shown < 20:
                print(f'[0x{opcode:02X} STRING  @ 0x{pos:X}]: {text[:60]}')
                shown += 1
        pos += 1 + len(raw) + 1
        
    elif 0x63 <= opcode <= 0x67:  # EncStr range
        raw = decrypt_string(data, pos + 1)
        try:
            text = raw.decode('shift-jis', errors='replace')
        except:
            text = repr(raw[:30])
        if text.strip():
            enc_texts += 1
            if shown < 20:
                print(f'[0x{opcode:02X} ENCSTR @ 0x{pos:X}]: {text[:60]}')
                shown += 1
        # calc token length: find null in raw (encrypted) data
        end = pos + 1
        while end < len(data) and data[end] != 0:
            end += 1
        pos = end + 1
        
    elif 0x00 <= opcode <= 0x38:  # Uint8x2
        pos += 3
    elif 0x39 <= opcode <= 0x43:  # Uint8Str
        raw = read_string(data, pos + 2)
        pos += 2 + len(raw) + 1
    elif 0x68 <= opcode <= 0xFF:  # Uint16x4
        pos += 9
    else:
        pos += 1
    count += 1

print()
print(f'Total STRING (0x44-0x62) texts shown: {str_texts}')
print(f'Total ENCSTR (0x63-0x67) texts shown: {enc_texts}')
