import os

filepath = r'E:\Games\DC3DD\DC3DDMesTextTool_GUI - Copy berantakan\DC3DDPatch a\src\dc4patch.cpp'
with open(filepath, 'r', encoding='utf-8') as f:
    content = f.read()

# Make the changes
content = content.replace("buffer[i] = L' '", "buffer[i] = L'X'")
content = content.replace("uChar = L' '", "uChar = L'X'")
content = content.replace("uChar = ' '", "uChar = 'X'")
content = content.replace("replacement = ' '", "replacement = 'X'")

with open(filepath, 'w', encoding='utf-8') as f:
    f.write(content)
print("Replaced space substitutes with X.")
