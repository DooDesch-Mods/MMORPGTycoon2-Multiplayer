"""Start isolated, owned local multiplayer test processes on Windows."""
from __future__ import annotations

import argparse
import ctypes
from ctypes import wintypes
import hashlib
import json
import os
from pathlib import Path
import secrets
import shutil
import socket
import subprocess
import sys
import time
import uuid

ROOT = Path(sys.executable).resolve().parent if getattr(sys, "frozen", False) else Path(__file__).resolve().parents[1]
DEFAULT_SOURCE = Path(r"F:\Launcher\SteamLibrary\steamapps\common\MMORPG Tycoon 2")
EXPECTED_EXE_SHA256 = "bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6"
OVERLAYS = ("MT2_Multiplayer.dll", "steam_api64.dll", "mt2mp_sidecar.exe")
MOD_UI_ASSETS = ("entry.win", "lobby.win", "browser.win", "multiplayer.png",
                 "materials/mt2mp_desktop_multiplayer.mat")
STEAM_APP_ID = "486860"


class LaunchError(RuntimeError):
    pass


def log(level, message):
    print(f"{level}: {message}", flush=True)


def atomic_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + ".tmp")
    temporary.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")
    os.replace(temporary, path)


def read_json(path):
    return json.loads(path.read_text(encoding="utf-8"))


def read_status_json(path):
    """Allow the game's atomic status replacement while this handle is open."""
    if os.name != "nt":
        return read_json(path)
    api = ctypes.WinDLL("kernel32", use_last_error=True)
    api.CreateFileW.argtypes = [wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD, ctypes.c_void_p,
                               wintypes.DWORD, wintypes.DWORD, wintypes.HANDLE]
    api.CreateFileW.restype = wintypes.HANDLE
    api.ReadFile.argtypes = [wintypes.HANDLE, ctypes.c_void_p, wintypes.DWORD, ctypes.POINTER(wintypes.DWORD), ctypes.c_void_p]
    api.CloseHandle.argtypes = [wintypes.HANDLE]
    handle = api.CreateFileW(str(path), 0x80000000, 0x1 | 0x2 | 0x4, None, 3, 0x80, None)
    if handle == ctypes.c_void_p(-1).value:
        raise ctypes.WinError(ctypes.get_last_error())
    try:
        chunks, size = [], 0
        buffer = ctypes.create_string_buffer(65536)
        count = wintypes.DWORD()
        while True:
            if not api.ReadFile(handle, buffer, len(buffer), ctypes.byref(count), None):
                raise ctypes.WinError(ctypes.get_last_error())
            if not count.value:
                break
            chunks.append(buffer.raw[:count.value])
            size += count.value
            if size > 1024 * 1024:
                raise ValueError(f"status file exceeds 1048576 bytes: {path}")
    finally:
        if not api.CloseHandle(handle):
            log("error", f"close status handle path={path} failed: {ctypes.WinError(ctypes.get_last_error())}")
    return json.loads(b"".join(chunks).decode("utf-8"))


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def same_path(left, right):
    return os.path.normcase(os.path.normpath(str(left))) == os.path.normcase(os.path.normpath(str(right)))


def safe_path(path, root):
    """Reject junctions and symlinks before writing into an owned runtime."""
    root, path = Path(os.path.abspath(root)), Path(os.path.abspath(path))
    if not path.is_relative_to(root):
        raise LaunchError(f"path outside test root: {path}")
    for item in (root, *path.relative_to(root).parents):
        candidate = item if item == root else root / item
        if candidate.is_symlink() or (hasattr(candidate, "is_junction") and candidate.is_junction()):
            raise LaunchError(f"runtime path redirects outside its directory: {candidate}")
    if path.is_symlink() or (hasattr(path, "is_junction") and path.is_junction()):
        raise LaunchError(f"runtime path redirects outside its directory: {path}")
    return path


class WindowsProcesses:
    QUERY = 0x1000
    TERMINATE = 0x0001
    SYNCHRONIZE = 0x00100000

    def __init__(self):
        if os.name != "nt":
            raise LaunchError("local game launch requires Windows")
        self.api = ctypes.WinDLL("kernel32", use_last_error=True)
        self.api.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
        self.api.OpenProcess.restype = wintypes.HANDLE
        self.api.CloseHandle.argtypes = [wintypes.HANDLE]
        self.api.GetProcessTimes.argtypes = [wintypes.HANDLE] + [ctypes.POINTER(wintypes.FILETIME)] * 4
        self.api.QueryFullProcessImageNameW.argtypes = [wintypes.HANDLE, wintypes.DWORD, wintypes.LPWSTR, ctypes.POINTER(wintypes.DWORD)]
        self.api.GetExitCodeProcess.argtypes = [wintypes.HANDLE, ctypes.POINTER(wintypes.DWORD)]
        self.api.TerminateProcess.argtypes = [wintypes.HANDLE, wintypes.UINT]
        self.api.WaitForSingleObject.argtypes = [wintypes.HANDLE, wintypes.DWORD]

    def _identity(self, handle, pid):
        times = [wintypes.FILETIME() for _ in range(4)]
        if not self.api.GetProcessTimes(handle, *(ctypes.byref(value) for value in times)):
            raise ctypes.WinError(ctypes.get_last_error())
        buffer = ctypes.create_unicode_buffer(32768)
        size = wintypes.DWORD(len(buffer))
        if not self.api.QueryFullProcessImageNameW(handle, 0, buffer, ctypes.byref(size)):
            raise ctypes.WinError(ctypes.get_last_error())
        return {"pid": pid, "process_start_time": str((times[0].dwHighDateTime << 32) | times[0].dwLowDateTime),
                "executable_path": buffer.value}

    def identity(self, process):
        return self._identity(int(process._handle), process.pid)

    def _exited(self, handle, pid):
        wait = self.api.WaitForSingleObject(handle, 0)
        if wait not in (0, 258):
            raise LaunchError(f"process state pid={pid} wait={wait} required=0 or 258")
        if wait == 0:
            code = wintypes.DWORD()
            if not self.api.GetExitCodeProcess(handle, ctypes.byref(code)):
                raise ctypes.WinError(ctypes.get_last_error())
            return f"exited:{code.value}"
        return None

    def inspect(self, record, terminate=False):
        rights = self.QUERY | self.SYNCHRONIZE | (self.TERMINATE if terminate else 0)
        handle = self.api.OpenProcess(rights, False, record["pid"])
        if not handle:
            error = ctypes.get_last_error()
            if error == 87:
                return "exited"
            raise LaunchError(f"OpenProcess pid={record['pid']} failed: {ctypes.WinError(error)}")
        try:
            exited = self._exited(handle, record["pid"])
            if exited:
                return exited
            try:
                identity = self._identity(handle, record["pid"])
            except OSError:
                exited = self._exited(handle, record["pid"])
                if exited:
                    return exited
                raise
            if identity["process_start_time"] != record["process_start_time"] or not same_path(identity["executable_path"], record["executable_path"]):
                return "identity-mismatch"
            exited = self._exited(handle, record["pid"])
            if exited:
                return exited
            if terminate:
                if not self.api.TerminateProcess(handle, 1):
                    raise ctypes.WinError(ctypes.get_last_error())
                result = self.api.WaitForSingleObject(handle, 5000)
                if result != 0:
                    raise LaunchError(f"termination pid={record['pid']} wait={result} required=0")
                code = wintypes.DWORD()
                if not self.api.GetExitCodeProcess(handle, ctypes.byref(code)):
                    raise ctypes.WinError(ctypes.get_last_error())
                return f"stopped:{code.value}"
            return "alive"
        finally:
            if not self.api.CloseHandle(handle):
                log("error", f"close process handle pid={record['pid']} failed: {ctypes.WinError(ctypes.get_last_error())}")


class RuntimeLock:
    def __init__(self, root):
        self.name = "Local\\MT2MP-" + hashlib.sha256(str(root.resolve()).casefold().encode()).hexdigest()

    def __enter__(self):
        self.api = ctypes.WinDLL("kernel32", use_last_error=True)
        self.api.CreateMutexW.argtypes = [ctypes.c_void_p, wintypes.BOOL, wintypes.LPCWSTR]
        self.api.CreateMutexW.restype = wintypes.HANDLE
        self.api.WaitForSingleObject.argtypes = [wintypes.HANDLE, wintypes.DWORD]
        self.api.ReleaseMutex.argtypes = [wintypes.HANDLE]
        self.api.CloseHandle.argtypes = [wintypes.HANDLE]
        self.handle = self.api.CreateMutexW(None, False, self.name)
        if not self.handle:
            raise ctypes.WinError(ctypes.get_last_error())
        result = self.api.WaitForSingleObject(self.handle, 0)
        if result not in (0, 0x80):
            self.api.CloseHandle(self.handle)
            raise LaunchError(f"test launcher/stop already operates on this runtime: wait={result}")
        return self

    def __exit__(self, *exc):
        if not self.api.ReleaseMutex(self.handle):
            log("error", f"release runtime lock failed: {ctypes.WinError(ctypes.get_last_error())}")
        if not self.api.CloseHandle(self.handle):
            log("error", f"close runtime lock failed: {ctypes.WinError(ctypes.get_last_error())}")


def validate_manifest(manifest, root):
    if manifest.get("schema_version") != 1 or manifest.get("owner") != {"kind": "MPTest", "run_id": manifest.get("run_id")}:
        raise LaunchError("invalid local runtime manifest ownership")
    uuid.UUID(manifest["run_id"])
    if not same_path(manifest.get("test_root", ""), root):
        raise LaunchError("manifest test_root does not match selected runtime")
    for record in manifest.get("processes", []):
        if record.get("owner") != manifest["owner"] or record.get("run_id") != manifest["run_id"]:
            raise LaunchError("process record ownership mismatch")
        if type(record.get("pid")) is not int or record["pid"] <= 0 or not isinstance(record.get("process_start_time"), str) or not record["process_start_time"].isdigit():
            raise LaunchError("invalid process identity record")
        runtime = safe_path(Path(record["runtime_root"]), root)
        executable = safe_path(Path(record["executable_path"]), root)
        if executable.parent != runtime / "game" or executable.name not in ("MT2.exe", "mt2mp_sidecar.exe"):
            raise LaunchError("manifest process executable is not an owned game/sidecar")


def check_inactive(root, processes):
    manifest_path = safe_path(root / "run.json", root)
    if not manifest_path.exists():
        return
    manifest = read_json(manifest_path)
    validate_manifest(manifest, root)
    for record in manifest["processes"]:
        state = processes.inspect(record)
        if state == "alive":
            raise LaunchError(f"previous run still owns live {record['role']} pid={record['pid']}; use stop_local_test.bat")
        log("info", f"previous {record['role']} pid={record['pid']} state={state}")
    return manifest


def archive_previous(root, manifest):
    if manifest is None:
        return
    archive = safe_path(root / "runs" / manifest["run_id"], root)
    manifest_path = safe_path(archive / "run.json", root)
    atomic_json(manifest_path, manifest)
    runtimes = {Path(record["runtime_root"]) for record in manifest["processes"]}
    copied = 0
    for runtime in runtimes:
        for relative in ("instance.json", "diagnostics/status.json", "diagnostics/response.json", "game/mt2mp.log", "game/mt2mp_proxy_debug.log", "game/MT2.RPT"):
            source = safe_path(runtime / relative, root)
            if source.is_file():
                destination = safe_path(archive / runtime.name / relative, root)
                destination.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(source, destination)
                copied += 1
    log("info", f"previous run evidence archived run={manifest['run_id']} files={copied} manifest={manifest_path}")


def source_files(source, build, *, original_steam=None):
    executable = source / "MT2.exe"
    if not executable.is_file():
        raise LaunchError(f"game executable missing: {executable}")
    actual = sha256(executable)
    if actual != EXPECTED_EXE_SHA256:
        raise LaunchError(f"unsupported MT2.exe SHA256 actual={actual} required={EXPECTED_EXE_SHA256}")
    files = {"MT2.exe": executable}
    for path in source.glob("*.dll"):
        if path.name.casefold() not in ("steam_api64.dll", "mt2_multiplayer.dll"):
            files[path.name] = path
    original_steam = original_steam or source / "steam_api64_original.dll"
    if not original_steam.is_file():
        raise LaunchError(f"original game dependency missing: {original_steam}")
    files["steam_api64_original.dll"] = original_steam
    for name in ("curl-ca-bundle.crt",):
        if (source / name).is_file():
            files[name] = source / name
    data = source / "Data"
    if not data.is_dir():
        raise LaunchError(f"game assets missing: {data}")
    for path in data.rglob("*"):
        if path.is_symlink() or (hasattr(path, "is_junction") and path.is_junction()):
            raise LaunchError(f"game asset redirects through link: {path}")
        if path.is_file():
            files[str(path.relative_to(source))] = path
    for name in OVERLAYS:
        path = build / name
        if not path.is_file():
            raise LaunchError(f"build output missing: {path}; run tools/build.py")
        files[name] = path
    for name in MOD_UI_ASSETS:
        path = ROOT / "mt2mp_ui" / name
        if not path.is_file():
            raise LaunchError(f"native Multiplayer UI asset missing: {path}")
        files["mt2mp_ui/" + name] = path
    for path in files.values():
        if path.is_symlink():
            raise LaunchError(f"source dependency is a symlink: {path}")
    return files


def prepare_instance(runtime, root, files):
    game = safe_path(runtime / "game", root)
    game.mkdir(parents=True, exist_ok=True)
    for name in ("data", "diagnostics", "logs"):
        safe_path(runtime / name, root).mkdir(parents=True, exist_ok=True)
    cache_path = safe_path(runtime / "copy-manifest.json", root)
    cache = read_json(cache_path) if cache_path.exists() else {}
    updated, copied = {}, 0
    for relative, source in files.items():
        destination = safe_path(game / relative, root)
        metadata = {"source": str(source.resolve()), "size": source.stat().st_size, "mtime_ns": source.stat().st_mtime_ns}
        previous = cache.get(relative, {})
        if all(previous.get(key) == value for key, value in metadata.items()) and destination.is_file():
            stat = destination.stat()
            if stat.st_size == metadata["size"] and stat.st_mtime_ns == previous.get("destination_mtime_ns"):
                updated[relative] = previous
                continue
        digest = sha256(source)
        destination.parent.mkdir(parents=True, exist_ok=True)
        if not destination.is_file() or destination.stat().st_size != metadata["size"] or sha256(destination) != digest:
            shutil.copy2(source, destination)
            copied += 1
        updated[relative] = {**metadata, "sha256": digest, "destination_mtime_ns": destination.stat().st_mtime_ns}
    atomic_json(cache_path, updated)
    safe_path(game / "steam_appid.txt", root).write_text(STEAM_APP_ID + "\n", encoding="ascii")
    log("info", f"runtime={runtime.name} copied={copied} assets={len(files)}")


def reserve_ports(ports):
    sockets = []
    try:
        for port in ports:
            endpoint = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sockets.append(endpoint)
            if hasattr(socket, "SO_EXCLUSIVEADDRUSE"):
                endpoint.setsockopt(socket.SOL_SOCKET, socket.SO_EXCLUSIVEADDRUSE, 1)
            endpoint.bind(("127.0.0.1", port))
            endpoint.listen(1)
        return sockets
    except OSError as error:
        for endpoint in sockets:
            endpoint.close()
        raise LaunchError(f"exclusive loopback port preflight failed port={port}: {error}") from error


def log_tail(path):
    if not path.exists():
        return "log not created"
    with path.open("rb") as stream:
        stream.seek(max(0, path.stat().st_size - 8192))
        return stream.read().decode("utf-8", errors="replace")


def check_processes(started):
    for process, record in started:
        code = process.poll()
        if code is not None:
            raise LaunchError(f"{record['role']} pid={process.pid} exited code={code}; log={record['log_path']}\n{log_tail(Path(record['log_path']))}")


def wait_marker(path, marker, started, timeout):
    deadline = time.monotonic() + timeout
    while True:
        check_processes(started)
        if marker in log_tail(path):
            log("info", f"{marker} acknowledged log={path}")
            return
        if time.monotonic() >= deadline:
            raise LaunchError(f"sidecar readiness timed out marker={marker} required={timeout}s log={path}\n{log_tail(path)}")
        time.sleep(0.1)


def game_readiness(status, record, expected_data):
    if status.get("schema_version") != 1:
        return "schema_version must be 1"
    if status.get("run_id") != record["run_id"] or status.get("pid") != record["pid"]:
        return "status run_id/pid does not match launched process"
    if status.get("game_thread_ready") is not True or not status.get("game_thread_id"):
        return "game thread has not acknowledged readiness"
    if status.get("session_connected") is not True:
        return f"session is not connected: {status.get('session_status', 'unavailable')}"
    actual_data = status.get("write_dir", "")
    if not actual_data or not Path(actual_data).is_absolute() or not Path(actual_data).resolve().is_relative_to(expected_data.resolve()):
        return f"PHYSFS write_dir={actual_data!r} required=inside {str(expected_data)!r}"
    return None


def wait_games(games, started, timeout):
    deadline, previous, read_failures = time.monotonic() + timeout, {}, {}
    while True:
        check_processes(started)
        waiting = []
        for record in games:
            path = Path(record["runtime_root"]) / "diagnostics" / "status.json"
            try:
                status = read_status_json(path)
                reason = game_readiness(status, record, Path(record["runtime_root"]) / "data")
            except (OSError, ValueError) as error:
                reason = f"status acknowledgement unavailable: {error}"
                failure = read_failures.setdefault(record["pid"], {"count": 0, "last_log": 0})
                failure["count"] += 1
                now = time.monotonic()
                if failure["count"] == 1 or now - failure["last_log"] >= 5:
                    log("warn", f"status read peer={record['peer_id']} count={failure['count']} path={path}: {error}")
                    failure["last_log"] = now
                waiting.append(f"peer={record['peer_id']} {reason}")
                continue
            if reason:
                waiting.append(f"peer={record['peer_id']} {reason}")
            if previous.get(record["pid"]) != reason:
                log("info", f"peer={record['peer_id']} {'waiting: ' + reason if reason else 'game thread, isolated saves and session acknowledged'}")
                previous[record["pid"]] = reason
        if not waiting:
            for record in games:
                failure = read_failures.get(record["pid"])
                if failure:
                    log("info", f"status read recovered peer={record['peer_id']} failures={failure['count']}")
            return
        if time.monotonic() >= deadline:
            raise LaunchError(f"game readiness timed out required={timeout}s; " + "; ".join(waiting))
        time.sleep(0.1)


def stop_manifest(root, processes):
    path = safe_path(root / "run.json", root)
    if not path.exists():
        log("info", f"no owned local run manifest: {path}")
        return 0
    manifest = read_json(path)
    validate_manifest(manifest, root)
    failures = 0
    for record in reversed(manifest["processes"]):
        try:
            state = processes.inspect(record, terminate=True)
            record["stop_state"] = state
            log("warn" if state == "identity-mismatch" else "info", f"stop {record['role']} pid={record['pid']} state={state}")
        except (OSError, LaunchError) as error:
            failures += 1
            record["stop_error"] = str(error)
            log("error", f"stop {record['role']} pid={record['pid']}: {error}")
    manifest["state"] = "stop-failed" if failures else "stopped"
    atomic_json(path, manifest)
    return 1 if failures else 0


def launch(args, processes):
    source, root, build = args.source.resolve(), Path(os.path.abspath(args.test_root)), args.build.resolve()
    if root.is_relative_to(source) or source.is_relative_to(root):
        raise LaunchError("source game and test root must be separate directories")
    safe_path(root, root)
    previous = check_inactive(root, processes)
    files = source_files(source, build)
    archive_previous(root, previous)
    runtimes = [root / ("host" if peer == 1 else f"client-{peer - 1}") for peer in range(1, args.players + 1)]
    for runtime in runtimes:
        prepare_instance(runtime, root, files)
    reservations = reserve_ports([args.relay_port, *range(args.control_port, args.control_port + args.players)])
    run_id, session, token = str(uuid.uuid4()), secrets.randbits(64) or 1, uuid.uuid4().hex
    manifest = {"schema_version": 1, "run_id": run_id, "owner": {"kind": "MPTest", "run_id": run_id},
                "test_root": str(root), "source_root": str(source), "session_id": str(session),
                "players": args.players, "relay_port": args.relay_port, "state": "starting", "processes": []}
    manifest_path = safe_path(root / "run.json", root)
    atomic_json(manifest_path, manifest)
    started = []

    def start(runtime, role, command, peer=0, env=None):
        installed_binary_sha256 = None
        if role == "game":
            copy_manifest = read_json(safe_path(runtime / "copy-manifest.json", root))
            installed_binary_sha256 = {}
            for name in ("MT2.exe", "MT2_Multiplayer.dll", "steam_api64.dll"):
                actual = sha256(safe_path(runtime / "game" / name, root))
                expected = copy_manifest.get(name, {}).get("sha256")
                if actual != expected:
                    raise LaunchError(f"installed binary hash differs file={name} actual={actual} required={expected}; game not started")
                installed_binary_sha256[name] = actual
        log_path = safe_path(runtime / "logs" / f"{role}-{run_id}.log", root)
        log("info", f"attempt launch role={role} peer={peer} executable={command[0]} log={log_path}")
        with log_path.open("wb") as stream:
            process = subprocess.Popen(command, cwd=runtime / "game", env=env, stdin=subprocess.DEVNULL,
                                       stdout=stream, stderr=subprocess.STDOUT,
                                       creationflags=subprocess.CREATE_BREAKAWAY_FROM_JOB | (subprocess.CREATE_NO_WINDOW if role != "game" else 0))
        provisional = {"role": role, "pid": process.pid, "log_path": str(log_path)}
        started.append((process, provisional))
        identity = processes.identity(process)
        record = {**identity, "schema_version": 1, "run_id": run_id, "owner": manifest["owner"],
                  "runtime_root": str(runtime), "role": role, "peer_id": peer, "log_path": str(log_path)}
        if role == "game":
            record["installed_binary_sha256"] = installed_binary_sha256
            record["game_log_paths"] = [str(runtime / "game" / name) for name in ("mt2mp.log", "mt2mp_proxy_debug.log", "MT2.RPT")]
        started[-1] = (process, record)
        manifest["processes"].append(record)
        atomic_json(manifest_path, manifest)
        if role == "game":
            atomic_json(safe_path(runtime / "instance.json", root), record)
        log("info", f"confirmed process role={role} peer={peer} pid={process.pid} creation={identity['process_start_time']}")
        return record

    try:
        reservations[0].close()
        relay = start(runtimes[0], "relay", [str(runtimes[0] / "game" / "mt2mp_sidecar.exe"), "--mode", "server",
                      "--relay-port", str(args.relay_port), "--session", str(session), "--token", token])
        wait_marker(Path(relay["log_path"]), "RELAY_READY", started, args.ready_timeout)
        games = []
        for peer, runtime in enumerate(runtimes, 1):
            control = args.control_port + peer - 1
            reservations[peer].close()
            bridge = start(runtime, "bridge", [str(runtime / "game" / "mt2mp_sidecar.exe"), "--mode", "client",
                           "--relay-host", "127.0.0.1", "--relay-port", str(args.relay_port), "--control-port", str(control),
                           "--session", str(session), "--token", token], peer)
            wait_marker(Path(bridge["log_path"]), "CONTROL_READY", started, args.ready_timeout)
            env = os.environ.copy()
            env.update({"MT2MP_TEST_ROOT": str(runtime / "data"), "MT2MP_RUNTIME_ROOT": str(runtime),
                        "MT2MP_RUN_ID": run_id, "MT2MP_PEER_ID": str(peer), "MT2MP_SESSION_ID": str(session),
                        "MT2MP_SESSION_TOKEN": token, "MT2MP_CONTROL_PORT": str(control),
                        "SteamAppId": STEAM_APP_ID, "SteamGameId": STEAM_APP_ID})
            games.append(start(runtime, "game", [str(runtime / "game" / "MT2.exe")], peer, env))
        wait_games(games, started, args.ready_timeout)
        manifest["state"] = "transport-ready"
        manifest["world_ready"] = False
        atomic_json(manifest_path, manifest)
        log("info", f"local transport ready players={args.players} run={run_id} manifest={manifest_path}")
        log("info", "shared simulation world remains unverified")
        return 0
    except BaseException as error:
        manifest["state"], manifest["failure"] = "failed", str(error)
        log("error", f"local launch failed: {error}")
        for process, record in reversed(started):
            try:
                if process.poll() is None:
                    process.kill()
                    process.wait(timeout=5)
                record["exit_code"] = process.returncode
                log("info", f"owned cleanup role={record['role']} pid={process.pid} exit={process.returncode} log={record['log_path']}")
            except (OSError, subprocess.TimeoutExpired) as cleanup_error:
                record["cleanup_error"] = str(cleanup_error)
                log("error", f"owned cleanup role={record['role']} pid={process.pid} failed: {cleanup_error}")
            for path_string in record.get("game_log_paths", []):
                path = Path(path_string)
                if path.is_file():
                    try:
                        log("error", f"launch diagnostics peer={record['peer_id']} log={path}\n{log_tail(path)}")
                    except OSError as log_error:
                        log("warn", f"launch diagnostics read log={path} failed: {log_error}")
        atomic_json(manifest_path, manifest)
        if isinstance(error, KeyboardInterrupt):
            return 130
        raise
    finally:
        for endpoint in reservations:
            endpoint.close()


def parser():
    result = argparse.ArgumentParser(description=__doc__)
    result.add_argument("-Players", "--players", type=int, choices=(2, 3, 4), default=2)
    result.add_argument("--source", type=Path, default=DEFAULT_SOURCE)
    result.add_argument("--test-root", type=Path, default=ROOT / "_MPTest")
    result.add_argument("--build", type=Path, default=ROOT / "build" / "bin" / "Release")
    result.add_argument("--relay-port", type=int, default=47010)
    result.add_argument("--control-port", type=int, default=47001)
    result.add_argument("--ready-timeout", type=float, default=60)
    return result


def main(argv=None):
    args = parser().parse_args(argv)
    if args.ready_timeout <= 0 or not (1 <= args.relay_port <= 65535) or not (1 <= args.control_port <= 65536 - args.players):
        log("error", "invalid timeout or port range")
        return 2
    if args.relay_port in range(args.control_port, args.control_port + args.players):
        log("error", "relay port overlaps a control port")
        return 2
    try:
        processes = WindowsProcesses()
        with RuntimeLock(args.test_root):
            return launch(args, processes)
    except (LaunchError, OSError, ValueError, KeyError, TypeError) as error:
        log("error", str(error))
        return 1


if __name__ == "__main__":
    sys.exit(main())
