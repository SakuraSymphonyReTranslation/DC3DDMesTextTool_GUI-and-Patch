import os
import json
import glob

def analyze_json_files(json_dir):
    unique_strings = set()
    total_strings = 0
    files = glob.glob(os.path.join(json_dir, 'sce_*.json'))
    
    for filepath in files:
        with open(filepath, 'r', encoding='utf-8-sig') as f:
            try:
                data = json.load(f)
                for entry in data:
                    msg = entry.get('message', '')
                    if msg:
                        total_strings += 1
                        unique_strings.add(msg)
            except json.JSONDecodeError:
                print(f"Failed to parse {filepath}")

    with open('sce_analysis.txt', 'a', encoding='utf-8') as out:
        out.write(f"Analysis of {os.path.basename(json_dir)}:\n")
        out.write(f"Total scenario string entries: {total_strings}\n")
        out.write(f"Unique scenario strings: {len(unique_strings)}\n")
        out.write("Sample strings:\n")
        for i, s in enumerate(list(unique_strings)[:10]):
            out.write(f"  {i+1}: {s}\n")
        out.write("-" * 40 + "\n")
        
        # Also write all unique strings at the end for reference
        out.write(f"ALL UNIQUE STRINGS FROM {os.path.basename(json_dir)}:\n")
        for s in sorted(list(unique_strings)):
            out.write(f"{s}\n")
        out.write("=" * 40 + "\n")

def main():
    # clear initial file
    with open('sce_analysis.txt', 'w', encoding='utf-8') as out:
        out.write("")
        
    dc4_dir = r"g:\Games\DC4\DC4Patch a saus kode 23-2\DC4Patch a saus kode 23-2\DC4Patch a"
    dc4ph_dir = r"g:\Games\DC4\DC4Patch a saus kode 23-2\DC4Patch a saus kode 23-2\DC4Patch a\dc4_sce\json_output"
    
    with open('sce_analysis.txt', 'a', encoding='utf-8') as out:
        out.write("--- Original DC4 JSON files ---\n")
    analyze_json_files(dc4_dir)
    
    with open('sce_analysis.txt', 'a', encoding='utf-8') as out:
        out.write("\n--- DC4 Plus Harmony JSON files ---\n")
    analyze_json_files(dc4ph_dir)

if __name__ == "__main__":
    main()
