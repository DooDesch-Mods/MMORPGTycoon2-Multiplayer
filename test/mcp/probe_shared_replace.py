"""Measure native replacement behavior without changing a game runtime."""
import ctypes
from ctypes import wintypes
import json
import os
from pathlib import Path
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools" / "mcp"))
from runtime import shared_open

kernel = ctypes.WinDLL("kernel32", use_last_error=True)
kernel.MoveFileExW.argtypes = [wintypes.LPCWSTR, wintypes.LPCWSTR, wintypes.DWORD]
kernel.ReplaceFileW.argtypes = [wintypes.LPCWSTR, wintypes.LPCWSTR, wintypes.LPCWSTR, wintypes.DWORD, ctypes.c_void_p, ctypes.c_void_p]
kernel.DeleteFileW.argtypes = [wintypes.LPCWSTR]
kernel.GetVolumeInformationW.argtypes = [wintypes.LPCWSTR, wintypes.LPWSTR, wintypes.DWORD, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p, wintypes.LPWSTR, wintypes.DWORD]
filesystem = ctypes.create_unicode_buffer(64)
volume_ok = kernel.GetVolumeInformationW(str(Path(__file__).resolve().anchor), None, 0, None, None, None, filesystem, 64)
results = {"filesystem": filesystem.value if volume_ok else {"error": ctypes.get_last_error()}, "operations": []}
with tempfile.TemporaryDirectory(dir=Path(__file__).resolve().parent) as directory:
    root = Path(directory)
    for operation in ("move", "replace", "delete"):
        target = root / f"{operation}.json"
        source = root / f"{operation}.tmp"
        target.write_bytes(b"old")
        source.write_bytes(b"new")
        with shared_open(target) as reader:
            if operation == "move":
                ok = kernel.MoveFileExW(str(source), str(target), 1)
            elif operation == "replace":
                ok = kernel.ReplaceFileW(str(target), str(source), None, 0, None, None)
            else:
                ok = kernel.DeleteFileW(str(target))
            results["operations"].append({"operation": operation, "ok": bool(ok), "error": 0 if ok else ctypes.get_last_error()})
print(json.dumps(results))
