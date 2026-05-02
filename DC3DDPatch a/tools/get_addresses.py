import os
import struct
from pathlib import Path


TARGETS = {
    "CheckIcon": 0x00432010,
    "CheckIconConfig": 0x004256A0,
    "BacklogIconHandler": 0x004325D0,
    "CallSite": 0x00405013,
    "HookSite": 0x00405276,
}


def read_u16(data: bytes, off: int) -> int:
    return struct.unpack_from("<H", data, off)[0]


def read_u32(data: bytes, off: int) -> int:
    return struct.unpack_from("<I", data, off)[0]


def parse_sections(data: bytes):
    pe_off = read_u32(data, 0x3C)
    if data[pe_off:pe_off + 4] != b"PE\x00\x00":
        raise ValueError("Invalid PE signature")

    num_sections = read_u16(data, pe_off + 6)
    opt_size = read_u16(data, pe_off + 20)
    opt_off = pe_off + 24
    magic = read_u16(data, opt_off)
    if magic != 0x10B:
        raise ValueError("Only PE32 is supported by this script")

    image_base = read_u32(data, opt_off + 28)
    sec_off = opt_off + opt_size
    sections = []
    for i in range(num_sections):
        off = sec_off + i * 40
        name = data[off:off + 8].rstrip(b"\x00").decode("ascii", errors="replace")
        virtual_size = read_u32(data, off + 8)
        virtual_address = read_u32(data, off + 12)
        raw_size = read_u32(data, off + 16)
        raw_ptr = read_u32(data, off + 20)
        sections.append({
            "name": name,
            "vsize": virtual_size,
            "vaddr": virtual_address,
            "raw_size": raw_size,
            "raw_ptr": raw_ptr,
        })
    return image_base, sections


def va_to_offset(va: int, image_base: int, sections) -> int | None:
    rva = va - image_base
    for s in sections:
        span = max(s["vsize"], s["raw_size"])
        if s["vaddr"] <= rva < s["vaddr"] + span:
            return s["raw_ptr"] + (rva - s["vaddr"])
    return None


def fmt_hex_bytes(blob: bytes) -> str:
    return " ".join(f"{b:02X}" for b in blob)


def main():
    default_exe = Path(__file__).resolve().parents[3] / "DC3DD.EXE"
    exe_path = Path(os.environ.get("DC3DD_EXE", str(default_exe)))
    if not exe_path.exists():
        print(f"File not found: {exe_path}")
        return 1

    data = exe_path.read_bytes()
    image_base, sections = parse_sections(data)

    print(f"EXE: {exe_path}")
    print(f"ImageBase: 0x{image_base:08X}")
    print("Sections:")
    for s in sections:
        print(
            f"  {s['name']:<8} RVA=0x{s['vaddr']:08X} "
            f"Raw=0x{s['raw_ptr']:08X} Size=0x{s['raw_size']:08X}"
        )

    print("\nOffset check:")
    for name, va in TARGETS.items():
        off = va_to_offset(va, image_base, sections)
        if off is None:
            print(f"  {name:<20} VA=0x{va:08X} -> not mapped")
            continue

        if off + 16 > len(data):
            print(f"  {name:<20} VA=0x{va:08X} -> off 0x{off:08X} (out of range)")
            continue

        sample = data[off:off + 16]
        print(
            f"  {name:<20} VA=0x{va:08X} -> off 0x{off:08X}  "
            f"bytes: {fmt_hex_bytes(sample)}"
        )

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
