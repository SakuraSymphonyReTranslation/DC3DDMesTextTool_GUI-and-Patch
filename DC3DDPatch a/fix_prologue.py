import os
import json
import glob
import subprocess

orig_dir = r"D:\dc4 tl project\original jp\original"
trans_dir = r"D:\dc4 tl project\translated"
out_json_dir = r"D:\dc4 tl project\fixed_json"
out_mes_dir = r"D:\vns\DC4\id_Data"

mes_importer = r"D:\DC4_Tool_Release\mes_json_importer.exe"

os.makedirs(out_json_dir, exist_ok=True)

# Find all sen files (prologue)
sen_files = glob.glob(os.path.join(trans_dir, "dc4_sen*.json"))

for trans_file in sen_files:
    fname = os.path.basename(trans_file)
    orig_file = os.path.join(orig_dir, fname)
    mes_orig_file = os.path.join(r"D:\vns\MES_JP", fname.replace(".json", ".mes"))
    out_json = os.path.join(out_json_dir, fname)
    out_mes = os.path.join(out_mes_dir, fname.replace(".json", ".mes"))

    if not os.path.exists(orig_file):
        print(f"Skipping {fname}: Original JSON not found.")
        continue
    
    if not os.path.exists(mes_orig_file):
         print(f"Skipping {fname}: Original MES not found.")
         continue

    try:
        with open(trans_file, 'r', encoding='utf-8-sig') as f:
            t_data = json.load(f)
            
        with open(orig_file, 'r', encoding='utf-8-sig') as f:
            o_data = json.load(f)
            
        for o, n in zip(o_data, t_data):
            om = o.get("message", "")
            nm = n.get("message", "")
            if om.startswith("\u3000") and not nm.startswith("\u3000"):
                n["message"] = "\u3000" + nm

        with open(out_json, "w", encoding="utf-8-sig") as f:
            json.dump(t_data, f, ensure_ascii=False, indent=2)

        # Run mes_json_importer without word wrap
        print(f"Compiling {fname} to MES...")
        subprocess.run([
            mes_importer, "import", 
            out_json, 
            mes_orig_file, 
            out_mes, 
            "-w", "0"
        ], check=True)

    except Exception as e:
        print(f"Error processing {fname}: {e}")

print("Done compiling prologue files!")
