import json

try:
    with open(r'd:\vns\DC4\indo.json', 'r', encoding='utf-8-sig') as f:
        data = json.load(f)
        
    with open(r'd:\vns\DC4\orig.json', 'r', encoding='utf-8-sig') as f:
        orig = json.load(f)

    for o, n in zip(orig, data):
        om = o.get('message', '')
        nm = n.get('message', '')
        if om.startswith('\u3000') and not nm.startswith('\u3000'):
            n['message'] = '\u3000' + nm

    with open(r'd:\vns\DC4\indo_fixed.json', 'w', encoding='utf-8-sig') as f:
        json.dump(data, f, ensure_ascii=False, indent=2)
    print("Success: indo_fixed.json")
except Exception as e:
    print(f"Error: {e}")
