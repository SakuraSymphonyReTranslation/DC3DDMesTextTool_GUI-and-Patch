"""Execute the shipped x86 hooks and DC3DD's native icon lookup in Unicorn.

Usage: python verify_dc3dd_layout.py GAME.EXE PATCH.dll audit_directory
Requires pefile, capstone, unicorn. Uses the matching linker's .map file.
Only CRT string helpers and body renderer calls are stubbed.
"""
import json
import re
import struct
import sys
from pathlib import Path
import capstone
import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import *

game_path, dll_path, audit_path = map(Path, sys.argv[1:4])
game, dll = pefile.PE(str(game_path)), pefile.PE(str(dll_path))
symbols = {}
for line in dll_path.with_suffix('.map').read_text().splitlines():
    match = re.match(r'\s+[0-9a-fA-F]+:[0-9a-fA-F]+\s+(\S+)\s+([0-9a-fA-F]{8})\b', line)
    if match:
        symbols[match[1]] = int(match[2], 16)
def sym(prefix):
    matches = [v for k,v in symbols.items() if k.startswith(prefix)]
    assert len(matches) == 1, (prefix, matches)
    return matches[0]

md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
def instructions(addr, count=160):
    return list(md.disasm(dll.get_data(addr-dll.OPTIONAL_HEADER.ImageBase, count), addr))
def settings(addr):
    ins = instructions(addr)
    flags = [int(re.search(r'0x[0-9a-f]+', i.op_str)[0], 16) for i in ins
             if i.mnemonic == 'cmp' and i.op_str.startswith('byte ptr [0x')]
    spacing = next(int(re.search(r'0x[0-9a-f]+', i.op_str)[0], 16) for i in ins
                   if i.mnemonic == 'mov' and i.op_str.startswith('eax, dword ptr [0x'))
    return flags[:2], spacing

u = Uc(UC_ARCH_X86, UC_MODE_32)
for pe in (game, dll):
    u.mem_map(pe.OPTIONAL_HEADER.ImageBase, (pe.OPTIONAL_HEADER.SizeOfImage+4095)&~4095)
    u.mem_write(pe.OPTIONAL_HEADER.ImageBase, pe.get_memory_mapped_image())
u.mem_map(0, 0x10000)
u.mem_map(0x20000000, 0x20000)
STACK, NAME, SCENARIO, STOP = 0x20008000, 0x20010000, 0x20011000, 0x2001f000
def put(addr, val): u.mem_write(addr, struct.pack('<I', val & 0xffffffff))
def get(addr): return struct.unpack('<I', u.mem_read(addr, 4))[0]
def cstr(addr):
    value = bytearray()
    for i in range(4096):
        b = u.mem_read(addr+i, 1)[0]
        if not b: return bytes(value)
        value.append(b)
    raise AssertionError('Unterminated string')
def string(addr, value): u.mem_write(addr, value+b'\0'+bytes(128))

put(sym('__tls_index'), 0)
put(0x2c, 0x1000)
put(0x1000, 0x2000)
tls = 0x2000 + sym('?bodySpacing@') - dll.DIRECTORY_ENTRY_TLS.struct.StartAddressOfRawData
registers = [UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDX,
             UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP]
def reset(args):
    for i,r in enumerate(registers): u.reg_write(r, 0x123400+i*0x100)
    u.reg_write(UC_X86_REG_ESP, STACK)
    u.reg_write(UC_X86_REG_EFLAGS, 0x246)
    put(STACK, STOP)
    for i,arg in enumerate(args): put(STACK+4+i*4, arg)
def finish(value, cleanup=0):
    sp = u.reg_read(UC_X86_REG_ESP)
    u.reg_write(UC_X86_REG_EAX, value & 0xffffffff)
    u.reg_write(UC_X86_REG_EIP, get(sp))
    u.reg_write(UC_X86_REG_ESP, sp+4+cleanup)

body_expected = None
def callbacks(u, address, size, data):
    sp = u.reg_read(UC_X86_REG_ESP)
    if address == 0x454260: # strstr
        a,b = get(sp+4),get(sp+8)
        at = cstr(a).find(cstr(b))
        finish(a+at if at >= 0 else 0)
    elif address == 0x456aed: # _strnicmp
        a,b,n = get(sp+4),get(sp+8),get(sp+12)
        aa,bb = cstr(a)[:n].lower(),cstr(b)[:n].lower()
        finish((aa > bb)-(aa < bb))
    elif address in (0x43fa40,0x417c70):
        expected_spacing, args = body_expected
        assert get(tls) == expected_spacing & 0xffffffff
        assert [get(sp+4+i*4) for i in range(len(args))] == args
        assert u.reg_read(UC_X86_REG_ECX) == 0x123600
        finish(0x12345678, 4*len(args))
u.hook_add(UC_HOOK_CODE, callbacks)

# Validate exact displaced instructions and all CALL targets before install.
sites = sym('?sites@dc3dd_layout')
for i in range(7):
    address,target,replacement = [get(sites+24*i+j) for j in (0,4,8)]
    if target:
        assert u.mem_read(address,1) == b'\xe8'
        assert (address+5+get(address+1)) & 0xffffffff == target
    else:
        assert bytes(u.mem_read(address,8)) == bytes.fromhex('8b4c2424014c2410')

# Line advance executes the real trampoline, TLS helper and original ADD.
line_cases = 0
for height in [0,10,11,12,24,34,44]:
    for spacing in [-2147483648,-20,-10,-5,0,2,64,2147483647]:
        reset([])
        put(STACK+0x24,height)
        put(STACK+0x10,99)
        put(tls,spacing)
        before = bytes(u.mem_read(STACK,128))
        u.emu_start(sym('?LineAdvanceHook@'),0x43f993,count=2000)
        advance = height if height < 12 else height+max(-10,min(64,spacing))
        assert get(STACK+0x10) == 99+advance
        assert get(STACK+0x24) == height
        after = bytearray(before)
        after[0x10:0x14] = struct.pack('<I',99+advance)
        assert bytes(u.mem_read(STACK,128)) == after
        assert u.reg_read(UC_X86_REG_ESP) == STACK
        for i,r in enumerate(registers):
            assert u.reg_read(r) == (advance if r == UC_X86_REG_ECX else 0x123400+i*0x100)
        flags = u.reg_read(UC_X86_REG_EFLAGS)
        put(STACK+0x10,99)
        put(STACK+0x24,advance)
        u.emu_start(0x43f98b,0x43f993,count=10)
        assert u.reg_read(UC_X86_REG_EFLAGS) == flags
        line_cases += 1

# Body wrappers preserve calling convention, options, ruby and outer TLS scope.
scope_cases = 0
for prefix, backlog in [('?DialogueText@',False),('?BacklogText@',True)]:
    addr = sym(prefix)
    (disable,advanced),spacing_addr = settings(addr)
    for off in (0,1):
        for enabled in (0,1):
            for text in (b'one\ntwo', b'abc\x81\x6f ruby', b'\x81\x81\x6f trail', b''):
                for spacing in (-10,0,5):
                    u.mem_write(disable,bytes([off]))
                    u.mem_write(advanced,bytes([enabled]))
                    put(spacing_addr,spacing)
                    string(NAME,text)
                    put(tls,7)
                    args = [10,20,0x87,NAME,99,0,1] if backlog else [NAME]
                    body_expected = (spacing if not off and enabled and text != b'abc\x81\x6f ruby' else 0,args)
                    reset(args)
                    u.emu_start(addr,STOP,count=3000)
                    assert get(tls) == 7
                    assert u.reg_read(UC_X86_REG_EAX) == 0x12345678
                    assert u.reg_read(UC_X86_REG_ESP) == STACK+4+4*len(args)
                    for i,r in enumerate(registers):
                        if r in (UC_X86_REG_EBX,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP):
                            assert u.reg_read(r) == 0x123400+i*0x100
                    scope_cases += 1

icon = sym('?BacklogIcon@')
icon_flag = next(int(re.search(r'0x[0-9a-f]+', i.op_str)[0],16) for i in instructions(icon)
                 if i.mnemonic == 'cmp' and i.op_str.startswith('byte ptr [0x'))
def lookup(name, scenario=b'', patched=False, enabled=True):
    string(NAME,name)
    string(SCENARIO,scenario)
    u.mem_write(icon_flag,bytes([enabled]))
    reset([NAME,SCENARIO])
    u.emu_start(icon if patched else 0x401590,STOP,count=100000)
    assert u.reg_read(UC_X86_REG_ESP) == STACK+12
    for i,r in enumerate(registers):
        if r in (UC_X86_REG_EBX,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP):
            assert u.reg_read(r) == 0x123400+i*0x100
    return u.reg_read(UC_X86_REG_EAX),cstr(NAME)

audit = json.loads((audit_path/'name_audit.json').read_text(encoding='utf-8'))
mapping = json.loads((audit_path/'supported_names.json').read_text(encoding='utf-8'))
icon_cases = 0
atlas = {}
for name in audit['names']:
    encoded = name.encode('cp932')
    expected = lookup(mapping[name].encode('cp932'))[0] if name in mapping else lookup(encoded)[0]
    assert lookup(encoded,patched=True) == (expected,encoded)
    assert lookup(encoded,patched=True,enabled=False) == lookup(encoded)
    icon_cases += 2
    if name in mapping:
        assert expected < 32
        atlas[name] = {'japanese':mapping[name], 'index':expected, 'y':expected//6,'x':expected%6}
        for altered in (encoded+b' Extra',encoded+b'_',encoded+b'X',b'X'*32):
            assert lookup(altered,patched=True) == lookup(altered)
            icon_cases += 1

# Every Japanese spelling, including input-rewriting Sakura variants, is native.
for ins in md.disasm(game.get_data(0x1590,0x6b0),0x401590):
    if ins.mnemonic == 'push' and ins.op_str.startswith('0x484'):
        name = game.get_data(int(ins.op_str,16)-0x400000,64).split(b'\0')[0]
        assert lookup(name,patched=True) == lookup(name)
        icon_cases += 1
for scenario in (b'',b'dd_3_0220_a1',b'dd_3_0220_a1x',b'DD_3_0220_A1X',b'dd_3_0220_a2x'):
    assert lookup(b'Otome',scenario,True)[0] == lookup(mapping['Otome'].encode('cp932'),scenario)[0]
    icon_cases += 1
assert lookup(b'',patched=True) == lookup(b'')
result = {'line_cases':line_cases,'scope_cases':scope_cases,'icon_cases':icon_cases+1,
          'supported_aliases':len(mapping),'normal_pitch':24,'large_pitch':34,
          'four_line_offsets':[0,24,48,72],'atlas':atlas}
(audit_path/'verification.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({k:v for k,v in result.items() if k != 'atlas'}))
