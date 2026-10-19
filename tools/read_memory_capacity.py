"""Read Windows physical memory before owned multi-instance acceptance."""
import argparse
import ctypes
from datetime import datetime, timezone
import json
from pathlib import Path

class MemoryStatus(ctypes.Structure):
    _fields_ = [('length', ctypes.c_uint32), ('load_percent', ctypes.c_uint32)] + [
        (name, ctypes.c_uint64) for name in ('total_physical_bytes', 'free_physical_bytes',
        'total_pagefile_bytes', 'free_pagefile_bytes', 'total_virtual_bytes',
        'free_virtual_bytes', 'extended_virtual_bytes')]

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
kernel = ctypes.WinDLL('kernel32', use_last_error=True)
kernel.GlobalMemoryStatusEx.argtypes = [ctypes.POINTER(MemoryStatus)]
kernel.GlobalMemoryStatusEx.restype = ctypes.c_int
value = MemoryStatus()
value.length = ctypes.sizeof(value)
if not kernel.GlobalMemoryStatusEx(ctypes.byref(value)):
    raise OSError(ctypes.get_last_error(), 'GlobalMemoryStatusEx failed; memory capacity unavailable')
assert value.total_physical_bytes > 0 and value.free_physical_bytes <= value.total_physical_bytes
record = {'operation': 'GlobalMemoryStatusEx', 'state': 'confirmed',
          'observed_utc': datetime.now(timezone.utc).isoformat(),
          'total_physical_bytes': value.total_physical_bytes,
          'free_physical_bytes': value.free_physical_bytes, 'load_percent': value.load_percent}
assert not args.output.exists(), 'Existing capacity observation must stay immutable'
args.output.parent.mkdir(parents=True, exist_ok=True)
args.output.write_text(json.dumps(record, indent=2) + '\n', encoding='utf-8')
print(json.dumps(record))
