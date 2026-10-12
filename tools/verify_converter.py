"""Integration tests for the built converter, using the local DC3DD corpus.

python tools/verify_converter.py GAME_ROOT TRANSLATED_JSON OUTPUT_DIRECTORY
Requires .NET 8 Desktop Runtime; source/translation files are never overwritten.
"""
import collections
import hashlib
import json
import re
import struct
import subprocess
import sys
from pathlib import Path

project = Path(__file__).resolve().parents[1]
game, translations, output = map(Path, sys.argv[1:4])
output.mkdir(parents=True, exist_ok=True)
dll = project / 'bin/Release/net8.0-windows/DC3DDMesTextTool_GUI.dll'
log = []

def run(*args, ok=True):
    result = subprocess.run(['dotnet', str(dll), *map(str, args)], capture_output=True, encoding='utf-8')
    log.append({'args': list(map(str, args)), 'exit': result.returncode, 'output': result.stdout + result.stderr})
    (output / 'commands.json').write_text(json.dumps(log, ensure_ascii=False, indent=2), encoding='utf-8')
    assert (result.returncode == 0) == ok, result.stdout + result.stderr

def read_json(path):
    return json.loads(path.read_text(encoding='utf-8-sig'))

def parse(path):
    """Independent bytecode scanner, including the native version-relative label oracle."""
    data = path.read_bytes()
    count = struct.unpack_from('<I', data)[0]
    version = count * 6 + 4
    assert data[version:version+3] == bytes.fromhex('a8a500')
    assert struct.unpack_from('<I', data, 4)[0] == 3
    pos = version + 3
    tokens, texts, targets = [], [], []
    while pos < len(data):
        op = data[pos]
        if op <= 0x38: end = pos + 3
        elif op <= 0x43: end = data.index(0, pos + 2) + 1
        elif op <= 0x67: end = data.index(0, pos + 1) + 1
        else: end = pos + 9
        assert end <= len(data)
        raw = data[pos:end]
        tokens.append((op, raw))
        if op in (3, 4): targets.append(end - version)
        if op == 0x58 or 0x63 <= op <= 0x67:
            string = raw[1:-1]
            if op >= 0x63: string = bytes((b + 32) & 255 for b in string)
            text = string.decode('cp932')
            if op >= 0x63: text = text.replace('_', ' ')
            if text: texts.append((op, text))
        pos = end
    assert len(targets) == count - 1
    labels = [v[0] for v in struct.iter_unpack('<I', data[8:8 + 4*(count-1)])]
    assert [v & 0xffffff for v in labels] == targets, path
    return data, version, tokens, texts, labels

run('batch-export', game / 'Translated/MES', output / 'JSON_Original')
run('batch-import', output / 'JSON_Original', game / 'Translated/MES', output / 'MES_Roundtrip', '--no-wrap')
original_files = sorted((game / 'Translated/MES').glob('*.mes'))
for path in original_files:
    assert path.read_bytes() == (output / 'MES_Roundtrip' / path.name).read_bytes(), path
    parse(path)
print(f'PASS: {len(original_files)} original MES files round-trip byte-for-byte.', flush=True)

# Exercise flat export/import and check that actual source opcodes determine name handling.
sample = game / 'Translated/MES/dd_7_0315_a1.mes'
# Reject unsupported bytecode versions before an export is written.
for version in (0x9872, 0xAAB6, 0xABB6, 0x9D72, 0xA09F):
    foreign = bytearray(sample.read_bytes())
    version_offset = struct.unpack_from('<I', foreign)[0] * 6 + 4
    struct.pack_into('<H', foreign, version_offset, version)
    input_path = output / f'unsupported_{version:04x}.mes'
    json_path = input_path.with_suffix('.json')
    input_path.write_bytes(foreign)
    run('export', input_path, json_path, ok=False)
    assert 'Unsupported or invalid MES header/version' in log[-1]['output']
    assert not json_path.exists()
print('PASS: five unsupported bytecode versions rejected without output.', flush=True)

run('export', sample, output / 'flat.json', '--flat')
run('import', output / 'flat.json', sample, output / 'flat.mes', '-w', '0')
assert sample.read_bytes() == (output / 'flat.mes').read_bytes()

rejected_sources = {
    'dd_1_0615_e2.json': 'character name exceeds the 31-byte field',
    'dd_7_0124_e1.json': 'character name exceeds the 31-byte field',
    'dd_7_0125_c1.json': 'name does not match a MES speaker/dialogue pair',
    'dd_7_0315_d1.json': 'name does not match a MES speaker/dialogue pair',
}
run('batch-import', translations, game / 'Translated/MES', output / 'MES_Indonesian', '-w', '60', ok=False)
errors = [line for line in log[-1]['output'].splitlines() if line.startswith('Error:')]
assert len(errors) == len(rejected_sources), errors
for name, reason in rejected_sources.items():
    assert any(name in error and reason in error for error in errors)
    assert not (output / 'MES_Indonesian' / (Path(name).stem + '.mes')).exists()
(output / 'rejected_sources.json').write_text(json.dumps(errors, ensure_ascii=False, indent=2), encoding='utf-8')
run('batch-export', output / 'MES_Indonesian', output / 'JSON_Reexported')
names = collections.Counter()
long_bodies = 0
normalizations = str.maketrans({'—': '―', 'û': 'u', 'ë': 'e', 'é': 'e', 'à': 'a'})
def canonical(text):
    # CP932 represents U+FF5E and U+301C using the same byte pair.
    return re.sub(r'\s+', '', text.translate(normalizations).replace('～', '〜').replace('－', '−'))

for path in sorted(translations.glob('*.json')):
    if path.name in rejected_sources: continue
    before = parse(game / 'Translated/MES' / (path.stem + '.mes'))
    after = parse(output / 'MES_Indonesian' / (path.stem + '.mes'))
    assert before[1] == after[1]
    assert before[0][:8] == after[0][:8]
    count = struct.unpack_from('<I', before[0])[0]
    assert before[0][8+4*(count-1):before[1]+3] == after[0][8+4*(count-1):after[1]+3]
    assert [v >> 24 for v in before[4]] == [v >> 24 for v in after[4]]
    assert len(before[2]) == len(after[2])
    for (op, raw), (new_op, new_raw) in zip(before[2], after[2]):
        assert op == new_op
        if op != 0x58 and not 0x63 <= op <= 0x67: assert raw == new_raw
    flat = []
    for row in read_json(path):
        if row.get('name'): flat.append(row['name'])
        flat.append(row['message'])
    assert len(flat) == len(after[3])
    for original, (op, actual) in zip(flat, after[3]):
        assert canonical(original) == canonical(actual), (path, original, actual)
        if op == 0x64:
            assert original == actual
            names[actual] += 1
        if op == 0x65 and '｛' not in actual:
            if len(actual.splitlines()) > 4: long_bodies += 1
            for line in actual.split('\n'):
                visible = re.sub(r'@[A-Za-z][0-9]*', '', line)
                width = sum(1 if ord(c) <= 127 or 0xff61 <= ord(c) <= 0xff9f else 2 for c in visible)
                assert width <= 60, (path, width, line)
    # Re-exported name/message JSON must retain every native text token in order.
    reexported = []
    for row in read_json(output / 'JSON_Reexported' / path.name):
        if row.get('name'): reexported.append(row['name'])
        reexported.append(row['message'])
    assert [canonical(s) for s in reexported] == [canonical(t) for _, t in after[3]]
translated_count = len(list(translations.glob('*.json'))) - len(rejected_sources)
print(f'PASS: {translated_count} translated files; text, names, instructions and labels verified. Four invalid source files rejected.', flush=True)

# Rejections must leave a pre-existing output untouched.
rows = read_json(output / 'JSON_Original' / (sample.stem + '.json'))

# Body wrapping keeps explicit line breaks, full-width text, controls and long words.
for body, expected in [
    ('one\ntwo', 'one\ntwo'),
    ('one\\ntwo', 'one\ntwo'),
    ('abcde fghij', 'abcde\nfghij'),
    ('A'*25, 'A'*10+'\n'+'A'*10+'\n'+'A'*5),
    ('@sabcdefghij@t90 klmnop', '@sabcdefghij@t90\nklmnop'),
    ('あいうえおか', 'あいうえお\nか'),
    ('｛ルビ／本文｝', '｛ルビ／本文｝'),
]:
    case = json.loads(json.dumps(rows))
    case[0]['message'] = body
    (output / 'wrap.json').write_text(json.dumps(case), encoding='utf-8')
    run('import', output / 'wrap.json', sample, output / 'wrap.mes', '-w', '10')
    assert parse(output / 'wrap.mes')[3][0][1] == expected
    assert [s for op, s in parse(output / 'wrap.mes')[3] if op == 0x64] == [s for op, s in parse(sample)[3] if op == 0x64]

sentinel = output / 'reject.mes'
negative_cases = [rows[:-1], rows + [rows[0]], [{'message': None}], [{'name': 42, 'message': 'x'}], {}]
for text in ['', 'bad\0text', '😀']:
    changed = json.loads(json.dumps(rows))
    changed[0]['message'] = text
    negative_cases.append(changed)
for name in ['A'*32, 'Kousuke\nExtra']:
    changed = json.loads(json.dumps(rows))
    next(row for row in changed if row.get('name'))['name'] = name
    negative_cases.append(changed)
for case in negative_cases:
    (output / 'invalid.json').write_text(json.dumps(case), encoding='utf-8')
    sentinel.write_bytes(b'KEEP')
    run('import', output / 'invalid.json', sample, sentinel, ok=False)
    assert sentinel.read_bytes() == b'KEEP'
run('import', output / 'flat.json', sample, output / 'bad-option.mes', '-w', 'oops', ok=False)
run('import', output / 'flat.json', sample, output / 'bad-option.mes', '-w', ok=False)
run('export', sample, sample, ok=False)
for bad in [b'\xff'*8, sample.read_bytes()[:-1]]:
    (output / 'truncated.mes').write_bytes(bad)
    run('export', output / 'truncated.mes', output / 'bad.json', ok=False)
empty_folder = output / 'empty'
empty_folder.mkdir(exist_ok=True)
run('batch-export', empty_folder, output / 'none', ok=False)
run('batch-import', translations, empty_folder, output / 'none', ok=False)
print('PASS: invalid input, count, encoding, name length, CLI options, and missing templates rejected.', flush=True)

summary = {'original_roundtrips': len(original_files), 'translated_files': translated_count,
           'rejected_source_files': list(rejected_sources), 'wrapping_cases': 7,
           'distinct_speakers': len(names), 'body_messages_over_four_lines': long_bodies,
           'dll_sha256': hashlib.sha256(dll.read_bytes()).hexdigest(), 'negative_cases': len(negative_cases)+7}
(output / 'verification.json').write_text(json.dumps(summary, indent=2), encoding='utf-8')
print(json.dumps(summary))
