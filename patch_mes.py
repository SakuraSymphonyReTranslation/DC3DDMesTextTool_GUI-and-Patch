import os

target = 'MesScript.cs'
with open(target, 'rb') as f:
    c = f.read().decode('utf-8', errors='ignore')

c = c.replace("text.Replace(' ', '_')", "text.Replace(' ', (char)0xA0)")

with open(target, 'wb') as f:
    f.write(c.encode('utf-8'))
print("Replaced '_' with '(char)0xA0'")
