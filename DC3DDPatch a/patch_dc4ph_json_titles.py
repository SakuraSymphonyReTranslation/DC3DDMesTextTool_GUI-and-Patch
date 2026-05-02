import os
import json
import glob

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
            translations[jp] = tr
    return translations

def patch_json_files():
    base_dir = r"g:\Games\DC4\DC4Patch a saus kode 23-2\DC4Patch a saus kode 23-2\DC4Patch a"
    trans_file = os.path.join(base_dir, 'translation_edit.txt')
    translations = load_translations(trans_file)
    print(f"Loaded {len(translations)} translations from translation_edit.txt")

    input_dir = os.path.join(base_dir, r'dc4_sce\json_output')
    output_dir = os.path.join(base_dir, r'dc4_sce\json_patched')
    
    os.makedirs(output_dir, exist_ok=True)
    files = glob.glob(os.path.join(input_dir, 'sce_*.json'))
    
    total_files = 0
    total_translated = 0
    total_skipped = 0
    
    for filepath in files:
        basename = os.path.basename(filepath)
        outpath = os.path.join(output_dir, basename)
        
        try:
            with open(filepath, 'r', encoding='utf-8-sig') as f:
                data = json.load(f)
        except json.JSONDecodeError:
            print(f"Skipping {basename} (failed to parse)")
            continue
            
        file_translated = 0
        for entry in data:
            msg = entry.get('message', '')
            if msg.startswith('@s◆') and msg.endswith('◆'):
                core_jp = msg[3:-1]
                if core_jp in translations:
                    # Update message with translated text. Retain the @s◆ and ◆ markers
                    # as required by the JSON to MES compiler.
                    entry['message'] = f"@s◆{translations[core_jp]}◆"
                    file_translated += 1
                else:
                    total_skipped += 1
        
        # Always write output, even if 0 translated, so all files are ready for rebuilding
        with open(outpath, 'w', encoding='utf-8-sig') as f:
            json.dump(data, f, ensure_ascii=False, indent=2)
            
        total_translated += file_translated
        total_files += 1
        print(f"{basename}: translated {file_translated} titles.")

    print(f"\nSaved {total_files} patched files to {output_dir}")
    print(f"Total entries translated: {total_translated}")
    print(f"Total entries skipped (missing translation): {total_skipped}")

if __name__ == '__main__':
    patch_json_files()
