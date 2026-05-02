import pefile
import sys
import os
import binascii

def va_to_file_offset(pe, va):
    return pe.get_offset_from_rva(va - pe.OPTIONAL_HEADER.ImageBase)

def file_offset_to_va(pe, offset):
    for section in pe.sections:
        if section.PointerToRawData <= offset < section.PointerToRawData + section.SizeOfRawData:
            rva = section.VirtualAddress + (offset - section.PointerToRawData)
            return pe.OPTIONAL_HEADER.ImageBase + rva
    return 0

def get_bytes(pe, file_data, va, length):
    offset = va_to_file_offset(pe, va)
    return file_data[offset:offset+length]

def find_pattern(file_data, pattern):
    return file_data.find(pattern)

def main():
    dc4_path = r"g:\Games\DC4\DC4.EXE"
    dc4ph_path = r"H:\Games\D.C.4 Da Capo 4 Plus Harmony\DC4PHDL.exe"

    if not os.path.exists(dc4_path):
        print(f"File not found: {dc4_path}")
        return
    if not os.path.exists(dc4ph_path):
        print(f"File not found: {dc4ph_path}")
        return

    pe1 = pefile.PE(dc4_path)
    pe2 = pefile.PE(dc4ph_path)

    with open(dc4_path, "rb") as f:
        data1 = f.read()
    with open(dc4ph_path, "rb") as f:
        data2 = f.read()

    targets = [
        ("CheckIcon", 0x004049D0, 32),
        ("CallSite", 0x00405013, 20),
        ("Real_BacklogFunc", 0x00404EE0, 32),
        ("HookSite", 0x00405276, 20)
    ]

    for name, va, length in targets:
        # Get signature from DC4
        try:
            offset = va_to_file_offset(pe1, va)
            pattern = data1[offset:offset+length]
            print(f"{name} (DC4 VA: {hex(va)}) Pattern: {binascii.hexlify(pattern).decode()}")
            
            # Find in DC4PH
            found_offset = data2.find(pattern)
            if found_offset != -1:
                found_va = file_offset_to_va(pe2, found_offset)
                print(f"  -> Found in DC4PH at offset {hex(found_offset)}, VA: {hex(found_va)}")
                
                # Double check uniqueness
                found_offset2 = data2.find(pattern, found_offset + 1)
                if found_offset2 != -1:
                    print(f"  -> WARNING: Pattern is not unique! Also found at {hex(found_offset2)}")
            else:
                # Try shorter pattern
                short_pattern = pattern[:length//2]
                found_offset = data2.find(short_pattern)
                if found_offset != -1:
                    found_va = file_offset_to_va(pe2, found_offset)
                    print(f"  -> Found shorter pattern in DC4PH at VA: {hex(found_va)}")
                else:
                    print(f"  -> Not found in DC4PH")
        except Exception as e:
            print(f"Error processing {name}: {e}")

    # Now for TABLE_BASE, finding references to 0x004BDA00
    print("\nLooking for TABLE_BASE references...")
    table_base_bytes = (0x004BDA00).to_bytes(4, byteorder='little')
    idx = 0
    while True:
        idx = data1.find(table_base_bytes, idx)
        if idx == -1:
            break
        # Look around this reference
        ref_va = file_offset_to_va(pe1, idx)
        if ref_va > 0:
            print(f"Found reference to TABLE_BASE in DC4 at VA {hex(ref_va)}")
            # Get 8 bytes before and 4 bytes after
            sig = data1[idx-8:idx+8]
            print(f"  Signature around ref: {binascii.hexlify(sig).decode()}")
            
            # Find in DC4PH by masking out the address
            # We will use the opcode bytes before the address
            opcode_sig = data1[idx-4:idx]
            print(f"  Opcode sig: {binascii.hexlify(opcode_sig).decode()}")
            
            # Find this opcode sig inside the new Real_BacklogFunc
            # (We will do this manually based on the output)
        idx += 1

if __name__ == '__main__':
    main()
