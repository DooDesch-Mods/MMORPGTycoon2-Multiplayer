"""Start one isolated Steam instance for private two-computer acceptance."""
import argparse
import ctypes
import os
from pathlib import Path
import secrets
import subprocess
import sys
import time
import uuid

import local_test as local


def steam_environment(runtime, run_id):
    env = os.environ.copy()
    env.update({"MT2MP_TEST_ROOT": str(runtime / "data"),
                "MT2MP_RUNTIME_ROOT": str(runtime), "MT2MP_RUN_ID": run_id,
                "MT2MP_PEER_ID": "1", "MT2MP_SESSION_ID": str(secrets.randbits(64) or 1),
                "MT2MP_SESSION_TOKEN": uuid.uuid4().hex, "MT2MP_CONTROL_PORT": "47001",
                "MT2MP_TRANSPORT": "steam", "SteamAppId": local.STEAM_APP_ID,
                "SteamGameId": local.STEAM_APP_ID})
    return env


def bootstrap_reason(status, record, data):
    # Lobby membership is created by the player after the main menu is ready.
    observed = dict(status, session_connected=True)
    reason = local.game_readiness(observed, record, data)
    if reason:
        return reason
    if not status.get("capabilities", {}).get("read_native_lobby"):
        return "owned native lobby profile has not acknowledged readiness"
    return None


def source_files(source, build):
    # local_test requires the preserved original name. A clean friend install
    # normally only has steam_api64.dll; stage that name in the isolated copy.
    original = source / "steam_api64_original.dll"
    if original.is_file():
        return local.source_files(source, build)
    # Avoid accepting the currently built proxy as an original Steam library.
    original = source / "steam_api64.dll"
    if not original.is_file() or local.sha256(original) == local.sha256(build / "steam_api64.dll"):
        raise local.LaunchError(f"original Steam library unavailable: {original}")
    # Reuse all asset/hash/link validation without writing to the game install.
    # source_files supports only preserved-name installations, so assemble a
    # temporary name view through an explicit original override below.
    return local.source_files(source, build, original_steam=original)


def launch(args, processes):
    root = Path(os.path.abspath(args.test_root))
    source = args.source.resolve()
    if root.name != "_MPTest":
        raise local.LaunchError("native snapshot profile requires test-root basename _MPTest")
    if root.is_relative_to(source) or source.is_relative_to(root):
        raise local.LaunchError("source game and Steam runtime must be separate directories")
    local.safe_path(root, root)
    previous = local.check_inactive(root, processes)
    files = source_files(source, args.build.resolve())
    local.archive_previous(root, previous)
    runtime = root / "host"
    local.prepare_instance(runtime, root, files)
    run_id = str(uuid.uuid4())
    owner = {"kind": "MPTest", "run_id": run_id}
    manifest = {"schema_version": 1, "run_id": run_id, "owner": owner,
                "test_root": str(root), "source_root": str(source), "players": 1,
                "transport": "steam", "state": "starting", "processes": []}
    manifest_path = root / "run.json"
    local.atomic_json(manifest_path, manifest)
    game = runtime / "game"
    log_path = runtime / "logs" / f"game-{run_id}.log"
    process = None
    try:
        binary_hashes = {name: local.sha256(game / name)
                         for name in ("MT2.exe", "MT2_Multiplayer.dll", "steam_api64.dll")}
        copied = local.read_json(runtime / "copy-manifest.json")
        for name, actual in binary_hashes.items():
            if copied[name]["sha256"] != actual:
                raise local.LaunchError(f"installed binary hash differs: {name}")
        if getattr(sys, "frozen", False):
            # External games must use their own DLL search path, not the
            # launcher's bundled Python runtime directory.
            api = ctypes.WinDLL("kernel32", use_last_error=True)
            api.SetDllDirectoryW.argtypes = [ctypes.c_wchar_p]
            api.SetDllDirectoryW.restype = ctypes.c_int
            if not api.SetDllDirectoryW(None):
                raise ctypes.WinError(ctypes.get_last_error())
        with log_path.open("wb") as stream:
            process = subprocess.Popen([str(game / "MT2.exe")], cwd=game,
                env=steam_environment(runtime, run_id), stdin=subprocess.DEVNULL,
                stdout=stream, stderr=subprocess.STDOUT,
                creationflags=subprocess.CREATE_BREAKAWAY_FROM_JOB)
        record = {**processes.identity(process), "schema_version": 1, "run_id": run_id,
                  "owner": owner, "runtime_root": str(runtime), "role": "game", "peer_id": 1,
                  "log_path": str(log_path), "installed_binary_sha256": binary_hashes,
                  "transport": "steam"}
        manifest["processes"].append(record)
        local.atomic_json(manifest_path, manifest)
        local.atomic_json(runtime / "instance.json", record)
        deadline = time.monotonic() + args.ready_timeout
        previous_reason = None
        while True:
            code = process.poll()
            if code is not None:
                raise local.LaunchError(f"Steam game exited code={code}; log={log_path}\n{local.log_tail(log_path)}")
            try:
                status = local.read_status_json(runtime / "diagnostics/status.json")
                reason = bootstrap_reason(status, record, runtime / "data")
            except (OSError, ValueError) as error:
                reason = f"status acknowledgement unavailable: {error}"
            if not reason:
                break
            if reason != previous_reason:
                local.log("info", f"Steam bootstrap waiting: {reason}")
                previous_reason = reason
            if time.monotonic() >= deadline:
                raise local.LaunchError(f"Steam bootstrap timed out required={args.ready_timeout}s: {reason}; native log:\n{local.log_tail(game / 'mt2mp.log')}")
            time.sleep(0.1)
        manifest["state"] = "menu-ready"
        manifest["session_connected"] = status.get("session_connected", False)
        local.atomic_json(manifest_path, manifest)
        local.log("info", f"Steam menu ready pid={process.pid} run={run_id}; physical lobby and world acceptance still required")
        return 0
    except BaseException as error:
        manifest["state"], manifest["failure"] = "failed", str(error)
        local.log("error", f"Steam bootstrap failed: {error}")
        if process is not None and process.poll() is None:
            process.kill()
            process.wait(timeout=5)
        local.atomic_json(manifest_path, manifest)
        raise


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path)
    parser.add_argument("--build", type=Path, default=local.ROOT if getattr(sys, "frozen", False) else local.ROOT / "build/bin/Release")
    parser.add_argument("--test-root", type=Path, default=local.ROOT / "_SteamTest" / "_MPTest")
    parser.add_argument("--ready-timeout", type=float, default=60)
    args = parser.parse_args()
    if args.source is None:
        if local.DEFAULT_SOURCE.is_dir():
            args.source = local.DEFAULT_SOURCE
        else:
            print("Steam must be running. Enter the folder containing MT2.exe (private saves are preserved):")
            try:
                selected = input().strip().strip(chr(34))
            except EOFError:
                local.log("error", "Game folder missing; pass --source")
                return 2
            if not selected:
                local.log("error", "No game folder provided")
                return 2
            args.source = Path(selected)
    if args.ready_timeout <= 0:
        parser.error("ready-timeout must be positive")
    try:
        processes = local.WindowsProcesses()
        with local.RuntimeLock(args.test_root):
            return launch(args, processes)
    except (local.LaunchError, OSError, ValueError, KeyError, TypeError) as error:
        local.log("error", str(error))
        return 1


if __name__ == "__main__":
    sys.exit(main())
