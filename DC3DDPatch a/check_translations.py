import os
import json
import glob
import re

def load_translations(txt_path):
    with open(txt_path, 'r', encoding='utf-8') as f:
        text = f.read()

    translations = {}
    blocks = text.split('\n\n')
    for block in blocks:
        lines = block.strip().split('\n')
        jp = ''
        tr = ''
        for line in lines:
            if line.startswith('Japanese: '):
                jp = line[10:].strip()
            elif line.startswith('Translation: '):
                tr = line[13:].strip()
        if jp and tr:
            # The Japanese string in translation_edit.txt is like ◆タイトル名◆
            # In the JSON it is like @s◆タイトル名◆
            # So let's store without exact markers or just as is
            translations[jp] = tr
    return translations

def check_coverage():
    base_dir = r"g:\Games\DC4\DC4Patch a saus kode 23-2\DC4Patch a saus kode 23-2\DC4Patch a"
    trans_file = os.path.join(base_dir, 'translation_edit.txt')
    translations = load_translations(trans_file)
    print(f"Loaded {len(translations)} translations from translation_edit.txt")

    dc4ph_dir = os.path.join(base_dir, r'dc4_sce\json_output')
    files = glob.glob(os.path.join(dc4ph_dir, 'sce_*.json'))
    
    unique_strings = set()
    for filepath in files:
        with open(filepath, 'r', encoding='utf-8-sig') as f:
            try:
                data = json.load(f)
                for entry in data:
                    msg = entry.get('message', '')
                    if msg.startswith('@s◆') and msg.endswith('◆'):
                        # typical scenario title
                        core_jp = msg[3:-1] # remove @s◆ and ◆ to match translation_edit.txt
                        unique_strings.add(core_jp)
            except json.JSONDecodeError:
                pass

    print(f"Found {len(unique_strings)} unique scenario titles in DC4PH.")
    missing = []
    for jp in unique_strings:
        if jp not in translations:
            missing.append(jp)
            
    if missing:
        print(f"Missing {len(missing)} translations! Sample:")
        for m in missing[:10]:
            print(f"  {m}")
    else:
        print("All DC4PH scenario titles are fully translated!")

if __name__ == '__main__':
    check_coverage()
