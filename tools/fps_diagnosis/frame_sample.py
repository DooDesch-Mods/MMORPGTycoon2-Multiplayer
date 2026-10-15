"""Read two existing status files and OS counters; no game mailbox requests."""
import argparse
import ctypes
import json
import sys
import time
from pathlib import Path
import psutil

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from local_test import read_status_json
parser = argparse.ArgumentParser()
parser.add_argument('--seconds', type=float, default=20)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--test-root', type=Path, default=ROOT / '_MPTest')
parser.add_argument('--expected-run-id')
args = parser.parse_args()
test_root = args.test_root.resolve()
test_root.relative_to((ROOT / '_MPTest').resolve())
if not 5 <= args.seconds <= 45:
    raise ValueError('Bounded sample must take 5 to 45 seconds')
user = ctypes.WinDLL('user32', use_last_error=True)
user.GetForegroundWindow.restype = ctypes.c_void_p
user.GetWindowThreadProcessId.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_ulong)]

def render_delta(prior, current, role):
    for value in (prior, current):
        if value.get('schema') != 1 or value.get('unit') != 'nanoseconds':
            raise RuntimeError(f'{role}: unexpected render counter contract')
        for key in ('completed', 'sampled_monotonic_ns', 'last_completed_monotonic_ns', 'window_identity'):
            number = value.get(key)
            if type(number) is not int or number < 0:
                raise RuntimeError(f'{role}: invalid render counter {key}')
        if value['last_completed_monotonic_ns'] > value['sampled_monotonic_ns']:
            raise RuntimeError(f'{role}: render completion timestamp exceeds sample')
        if bool(value['completed']) != bool(value['last_completed_monotonic_ns']) or \
                bool(value['completed']) != bool(value['window_identity']):
            raise RuntimeError(f'{role}: render completion lacks timestamp or window')
    if prior['window_identity'] != current['window_identity']:
        raise RuntimeError(f'{role}: rendered window identity changed')
    count = current['completed'] - prior['completed']
    ns = current['sampled_monotonic_ns'] - prior['sampled_monotonic_ns']
    if count < 0 or ns <= 0 or current['last_completed_monotonic_ns'] < prior['last_completed_monotonic_ns']:
        raise RuntimeError(f'{role}: render counter or clock rolled back')
    if count and current['last_completed_monotonic_ns'] <= prior['sampled_monotonic_ns']:
        raise RuntimeError(f'{role}: render completion did not occur after first sample')
    if not count and current['last_completed_monotonic_ns'] != prior['last_completed_monotonic_ns']:
        raise RuntimeError(f'{role}: render timestamp changed without completion')
    return {'completed': count, 'interval_seconds': ns / 1e9,
            'fps': count * 1e9 / ns,
            'first_sample_monotonic_ns': prior['sampled_monotonic_ns'],
            'last_sample_monotonic_ns': current['sampled_monotonic_ns'],
            'first_completed_monotonic_ns': prior['last_completed_monotonic_ns'],
            'last_completed_monotonic_ns': current['last_completed_monotonic_ns'],
            'window_identity': current['window_identity']}

def sample():
    rows = {}
    fg = ctypes.c_ulong()
    user.GetWindowThreadProcessId(user.GetForegroundWindow(), ctypes.byref(fg))
    for role in ('host', 'client-1'):
        manifest = json.loads((test_root / role / 'instance.json').read_text())
        if args.expected_run_id and manifest['run_id'] != args.expected_run_id:
            raise RuntimeError(f'{role}: sample run differs actual={manifest["run_id"]} required={args.expected_run_id}')
        p = psutil.Process(manifest['pid'])
        if Path(p.exe()).resolve() != Path(manifest['executable_path']).resolve():
            raise RuntimeError(f'{role}: executable identity differs')
        if abs((p.create_time() + 11644473600) * 10000000 - int(manifest['process_start_time'])) > 100:
            raise RuntimeError(f'{role}: process creation identity differs')
        path = test_root / role / 'diagnostics/status.json'
        status = read_status_json(path)
        if status['pid'] != p.pid or status['run_id'] != manifest['run_id']:
            raise RuntimeError(f'{role}: status process/run identity differs')
        timing = status['frame_timing']
        if timing['schema'] != 1 or timing['unit'] != 'nanoseconds':
            raise RuntimeError(f'{role}: unexpected timing contract')
        cpu = p.cpu_times()
        rows[role] = {'pid': p.pid, 'run_id': manifest['run_id'], 'dll_sha256': manifest['installed_binary_sha256']['MT2_Multiplayer.dll'],
                      'cpu': cpu.user + cpu.system, 'status_mtime_ns': path.stat().st_mtime_ns,
                      'frame_timing': timing, 'render_presents': status['render_presents'],
                      'load_identity': status['load_identity'],
                      'clock': status['clock'], 'live_state': status['typed_actions']['live_state']}
    return {'monotonic': time.perf_counter(), 'foreground_pid': fg.value, 'rows': rows}

first = sample()
time.sleep(args.seconds)
last = sample()
elapsed = last['monotonic'] - first['monotonic']
derived = {}
for role, b in last['rows'].items():
    a = first['rows'][role]
    if a['pid'] != b['pid'] or a['run_id'] != b['run_id'] or a['dll_sha256'] != b['dll_sha256']:
        raise RuntimeError(f'{role}: sample ownership changed')
    parts = {}
    for name, total in b['frame_timing']['parts'].items():
        prior = a['frame_timing']['parts'][name]
        count = total['count'] - prior['count']
        ns = total['total_ns'] - prior['total_ns']
        if count < 0 or ns < 0:
            raise RuntimeError(f'{role}: timing counters reset')
        parts[name] = {'count': count, 'total_ms': ns / 1e6, 'mean_ms': ns / count / 1e6 if count else None}
    interval = parts['frame_interval']
    reader = {k: b['load_identity'][k] - a['load_identity'][k] for k in
              ('observer_sample_count', 'observer_sample_total_ns', 'negative_cache_hits', 'negative_cache_misses', 'rtti_fallbacks', 'rtti_graph_attempts')}
    render = render_delta(a['render_presents'], b['render_presents'], role)
    derived[role] = {'game_update_hz': 1000 / interval['mean_ms'] if interval['mean_ms'] else None,
                     'render_fps': render['fps'], 'render_presents': render,
                     'one_core_percent': 100 * (b['cpu'] - a['cpu']) / elapsed,
                     'completed_game_updates': b['frame_timing']['completed_game_updates'] - a['frame_timing']['completed_game_updates'],
                     'parts': parts, 'load_identity_delta': reader}
report = {'elapsed_seconds': elapsed, 'first': first, 'last': last, 'derived': derived}
args.output.parent.mkdir(parents=True, exist_ok=True)
args.output.write_text(json.dumps(report, indent=2), encoding='utf-8')
print(json.dumps({'report': str(args.output), 'elapsed_seconds': elapsed, 'foreground_pids': [first['foreground_pid'], last['foreground_pid']], 'derived': derived}, indent=2))
