"""Map a retained owned-game crash report to measured module offsets."""
import argparse
import ctypes
from ctypes import wintypes
from pathlib import Path
import json
import re
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent / 'mcp'))
from runtime import Runtime, read_json

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--manifest', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
manifest = args.manifest.resolve()
runtime = Runtime(manifest.parent.parent)
runtime.bind(str(manifest))
owned = read_json(manifest)
kernel = ctypes.WinDLL('kernel32', use_last_error=True)
psapi = ctypes.WinDLL('psapi', use_last_error=True)
kernel.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
kernel.OpenProcess.restype = wintypes.HANDLE
kernel.CloseHandle.argtypes = [wintypes.HANDLE]
psapi.EnumProcessModulesEx.argtypes = [wintypes.HANDLE, ctypes.POINTER(ctypes.c_void_p), wintypes.DWORD, ctypes.POINTER(wintypes.DWORD), wintypes.DWORD]
psapi.GetModuleFileNameExW.argtypes = [wintypes.HANDLE, ctypes.c_void_p, wintypes.LPWSTR, wintypes.DWORD]
class ModuleInfo(ctypes.Structure):
    _fields_ = [('base', ctypes.c_void_p), ('size', wintypes.DWORD), ('entry', ctypes.c_void_p)]
psapi.GetModuleInformation.argtypes = [wintypes.HANDLE, ctypes.c_void_p, ctypes.POINTER(ModuleInfo), wintypes.DWORD]
handle = kernel.OpenProcess(0x410, False, owned['pid'])
if not handle:
    raise ctypes.WinError(ctypes.get_last_error())
try:
    modules = (ctypes.c_void_p * 1024)(); used = wintypes.DWORD()
    if not psapi.EnumProcessModulesEx(handle, modules, ctypes.sizeof(modules), ctypes.byref(used), 3):
        raise ctypes.WinError(ctypes.get_last_error())
    if used.value > ctypes.sizeof(modules):
        raise RuntimeError('module inventory exceeds owned diagnostic capacity')
    inventory = []
    for module in list(modules)[:used.value // ctypes.sizeof(ctypes.c_void_p)]:
        info = ModuleInfo(); name = ctypes.create_unicode_buffer(32768)
        if not psapi.GetModuleInformation(handle, module, ctypes.byref(info), ctypes.sizeof(info)) or not psapi.GetModuleFileNameExW(handle, module, name, len(name)):
            raise ctypes.WinError(ctypes.get_last_error())
        inventory.append({'name': Path(name.value).name, 'base': info.base, 'size': info.size})
    report = manifest.parent / 'game/crash.rpt'
    frames = []
    for address in re.findall(r'0x([0-9a-fA-F]+)', report.read_text(encoding='utf-8', errors='replace')):
        low = int(address, 16)
        matches = [m for m in inventory if (low - (m['base'] & 0xffffffff)) & 0xffffffff < m['size']]
        frames.append({'low_address': hex(low), 'matches': [{'module': m['name'], 'rva': hex((low - (m['base'] & 0xffffffff)) & 0xffffffff)} for m in matches]})
    value = {'run_id': owned['run_id'], 'pid': owned['pid'], 'report': str(report), 'frames': frames}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(value, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(value, indent=2))
finally:
    kernel.CloseHandle(handle)
