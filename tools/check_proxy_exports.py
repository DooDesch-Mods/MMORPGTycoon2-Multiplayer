from pathlib import Path
import ctypes
import re

root = Path(__file__).resolve().parents[1]
source = root / 'proxy/steam_stubs_init.cpp'
names = re.findall(r'GetProcAddress\(original, "([^"]+)"\)', source.read_text())
original = Path(r'F:\Launcher\SteamLibrary\steamapps\common\MMORPG Tycoon 2\steam_api64_original.dll')
library = ctypes.WinDLL(str(original))
kernel = ctypes.WinDLL('kernel32', use_last_error=True)
kernel.GetProcAddress.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
kernel.GetProcAddress.restype = ctypes.c_void_p
missing = [name for name in names if not kernel.GetProcAddress(library._handle, name.encode())]
print(f'exports checked={len(names)} missing={len(missing)}')
for name in missing:
    print('error: missing original Steam export:', name)
raise SystemExit(bool(missing))
