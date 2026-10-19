from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
path = root / 'proxy/steam_stubs_init.cpp'
text = path.read_text(encoding='utf-8')
if 'bool fill_steam_pointers' not in text:
    text = text.replace('#include <windows.h>', '#include <windows.h>\n#include <cstdio>')
    text = text.replace('void fill_steam_pointers(HMODULE original) {', 'bool fill_steam_pointers(HMODULE original) {\n    bool complete = true;')
    text = re.sub(r'(    (\w+_ptr) = \(void\*\)GetProcAddress\(original, "([^"]+)"\);)',
                  lambda m: m[1] + '\n    if (!' + m[2] + ') { fprintf(stderr, "error: missing Steam export ' + m[3] + '\\n"); complete = false; }', text)
    pos = text.rfind('}')
    text = text[:pos] + '    return complete;\n' + text[pos:]
    path.write_text(text, encoding='utf-8')
    print('info: proxy export guards added')
else:
    print('info: proxy guards already present')
