import re

with open('src/dc4patch.cpp', 'r', encoding='utf-8') as f:
    lines = f.readlines()

new_lines = []
for line in lines:
    if not re.match(r'^\s*#include\s+<windows\.h>', line):
        new_lines.append(line)

new_lines.insert(0, '#include <windows.h>\n')

with open('src/dc4patch.cpp', 'w', encoding='utf-8') as f:
    f.writelines(new_lines)
