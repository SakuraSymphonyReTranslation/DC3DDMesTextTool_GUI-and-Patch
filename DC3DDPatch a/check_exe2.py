"""
Parse the most recent Windows minidump to extract crash EIP.
"""
import struct, sys, glob, os
sys.stdout.reconfigure(encoding='utf-8')

dumps = glob.glob("d:/vns/DC4/*.dmp")
dumps.sort(key=os.path.getmtime, reverse=True)
dump_path = dumps[0]
print(f"Most recent dump: {dump_path}")
print(f"  Time: {os.path.getmtime(dump_path)}")

data = open(dump_path, 'rb').read()
num_streams = struct.unpack_from('<I', data, 8)[0]
stream_rva  = struct.unpack_from('<I', data, 12)[0]

exception_rva = None
for i in range(num_streams):
    off = stream_rva + i * 12
    stream_type = struct.unpack_from('<I', data, off)[0]
    rva         = struct.unpack_from('<I', data, off+8)[0]
    if stream_type == 6:  # ExceptionStream
        exception_rva = rva
        break

if exception_rva:
    thread_id = struct.unpack_from('<I', data, exception_rva)[0]
    exc_base  = exception_rva + 8
    exc_code  = struct.unpack_from('<I', data, exc_base)[0]
    exc_addr  = struct.unpack_from('<Q', data, exc_base+16)[0]
    num_params = struct.unpack_from('<I', data, exc_base+24)[0]
    params = [struct.unpack_from('<Q', data, exc_base+28+i*8)[0] for i in range(min(num_params, 15))]

    codes = {0xC0000005:"ACCESS_VIOLATION", 0xC00000FD:"STACK_OVERFLOW",
             0xC000001D:"ILLEGAL_INSTRUCTION", 0xC0000409:"STACK_BUFFER_OVERRUN",
             0xC0000094:"INT_DIVIDE_BY_ZERO"}
    print(f"\nEXCEPTION:")
    print(f"  Code:    0x{exc_code:08X} ({codes.get(exc_code,'UNKNOWN')})")
    print(f"  Address: 0x{exc_addr:08X}  <- CRASH LOCATION")
    if exc_code == 0xC0000005 and num_params >= 2:
        print(f"  AV:      {'WRITE' if params[0]==1 else 'READ'} at 0x{params[1]:08X}")

    # Read context registers
    ctx_off  = exc_base + 28 + 15*8
    ctx_rva  = struct.unpack_from('<I', data, ctx_off+4)[0]
    if ctx_rva:
        segs_off = ctx_rva + 4 + 32 + 112
        eax_off  = segs_off + 20
        try:
            regs = [struct.unpack_from('<I', data, eax_off + i*4)[0] for i in range(9)]
            names = ['EAX','ECX','EDX','EBX','ESP','EBP','ESI','EDI','EIP']
            print("\nREGISTERS:")
            for n, v in zip(names, regs):
                print(f"  {n}: 0x{v:08X}", end="  ")
                if n in ('EIP','EAX','ECX','EDX','EBX','ESP','EBP'):
                    if 0x400000 <= v <= 0x600000:
                        print(f"(DC4.EXE+0x{v-0x400000:X})", end="")
            print()
            eip = regs[8]
            esp = regs[4]
            ebp = regs[5]
            print(f"\n  EIP in DC4.EXE at offset: 0x{eip-0x400000:X}")

            # Print stack contents
            print(f"\n  Stack @ ESP (0x{esp:08X}):")
            # Find stack memory in dump
            # Look for memory info stream (type 9) or memory64 (type 10)
            for i in range(num_streams):
                off2 = stream_rva + i*12
                stype = struct.unpack_from('<I', data, off2)[0]
                srva  = struct.unpack_from('<I', data, off2+8)[0]
                ssz   = struct.unpack_from('<I', data, off2+4)[0]
        except Exception as e:
            print(f"  Error reading regs: {e}")
