"""Read the launcher's Windows job ownership and lifetime flags."""
import ctypes
from ctypes import wintypes
import json
import os
import argparse

class BasicLimits(ctypes.Structure):
    _fields_ = [('process_time', ctypes.c_int64), ('job_time', ctypes.c_int64),
                ('flags', wintypes.DWORD), ('minimum_working_set', ctypes.c_size_t),
                ('maximum_working_set', ctypes.c_size_t), ('active_process_limit', wintypes.DWORD),
                ('affinity', ctypes.c_size_t), ('priority_class', wintypes.DWORD),
                ('scheduling_class', wintypes.DWORD)]

api = ctypes.WinDLL('kernel32', use_last_error=True)
api.GetCurrentProcess.restype = wintypes.HANDLE
api.IsProcessInJob.argtypes = [wintypes.HANDLE, wintypes.HANDLE, ctypes.POINTER(wintypes.BOOL)]
api.QueryInformationJobObject.argtypes = [wintypes.HANDLE, ctypes.c_int, ctypes.c_void_p, wintypes.DWORD, ctypes.POINTER(wintypes.DWORD)]
inside = wintypes.BOOL()
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--pid', type=int)
args = parser.parse_args()
api.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
api.OpenProcess.restype = wintypes.HANDLE
api.CloseHandle.argtypes = [wintypes.HANDLE]
handle = api.OpenProcess(0x1000, False, args.pid) if args.pid else api.GetCurrentProcess()
if not handle:
    raise ctypes.WinError(ctypes.get_last_error())
if not api.IsProcessInJob(handle, None, ctypes.byref(inside)):
    raise ctypes.WinError(ctypes.get_last_error())
result = {'pid':args.pid or os.getpid(), 'inside_job':bool(inside.value)}
if args.pid:
    if not api.CloseHandle(handle):
        raise ctypes.WinError(ctypes.get_last_error())
if inside.value and args.pid is None:
    limits = BasicLimits()
    size = wintypes.DWORD()
    if not api.QueryInformationJobObject(None, 2, ctypes.byref(limits), ctypes.sizeof(limits), ctypes.byref(size)):
        raise ctypes.WinError(ctypes.get_last_error())
    result.update(flags=limits.flags, kill_on_job_close=bool(limits.flags & 0x2000),
                  breakaway_allowed=bool(limits.flags & 0x800), silent_breakaway=bool(limits.flags & 0x1000))
print(json.dumps(result))
