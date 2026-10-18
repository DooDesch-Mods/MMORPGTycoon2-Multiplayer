"""Start or stop an owned two-peer Framework Multiplayer lab on Windows."""
from __future__ import annotations

import argparse
import hashlib
import math
import os
from pathlib import Path
import re
import secrets
import shutil
import subprocess
import sys
import time
import uuid

# The stage and promoted tool use the same existing process and lock implementation.
for ancestor in Path(__file__).resolve().parents:
    if (ancestor / "tools/local_test.py").is_file():
        sys.path.insert(0, str(ancestor / "tools"))
        break
import local_test as legacy
from local_test import (LaunchError, ROOT, DEFAULT_SOURCE, EXPECTED_EXE_SHA256, STEAM_APP_ID,
                        WindowsProcesses, RuntimeLock, atomic_json, read_json, read_status_json,
                        safe_path, sha256, log, reserve_ports, check_processes, wait_marker, log_tail,
                        )

MODULES = {"MT2.exe": "game/MT2.exe", "steam_api64.dll": "game/steam_api64.dll",
           "WSOCK32.dll": "game/WSOCK32.dll", "MT2Framework.dll": "game/MT2Framework.dll",
           "MT2MultiplayerPlugin.dll": "profile/mods/doodesch.multiplayer/MT2MultiplayerPlugin.dll"}


def validate_manifest(manifest, root):
    legacy.validate_manifest(manifest, root)
    for record in manifest["processes"]:
        if record["role"] != "game":
            if Path(record["executable_path"]).name != "mt2mp_sidecar.exe":
                raise LaunchError("sidecar record cannot select a game executable")
            continue
        runtime = Path(record["runtime_root"])
        hashes = record.get("installed_binary_sha256")
        if (record.get("schema_version") != 2 or record.get("runtime_kind") != "framework-plugin"
                or record.get("framework_profile") != str(runtime / "profile")
                or Path(record["executable_path"]) != runtime / "game/MT2.exe"
                or type(record.get("peer_id")) is not int or record["peer_id"] not in (1, 2)
                or not isinstance(hashes, dict) or set(hashes) != set(MODULES)
                or any(not isinstance(value, str) or not re.fullmatch(r"[0-9a-f]{64}", value) for value in hashes.values())):
            raise LaunchError("invalid exact Framework game identity receipt")
        safe_path(runtime / "profile", root)


def check_inactive(root, processes):
    path = safe_path(root / "run.json", root)
    if path.exists():
        validate_manifest(read_json(path), root)
    return legacy.check_inactive(root, processes)


def stop_manifest(root, processes):
    path = safe_path(root / "run.json", root)
    if path.exists():
        validate_manifest(read_json(path), root)
    return legacy.stop_manifest(root, processes)


def archive_previous(root, manifest):
    if manifest:
        # All old peer files already live inside this immutable per-run directory.
        archive = safe_path(root / "runs" / manifest["run_id"] / "run.json", root)
        atomic_json(archive, manifest)
        log("info", f"previous run and its private profile evidence retained: {archive}")


def checked_source(path):
    path = Path(os.path.abspath(path))
    safe_path(path, path)
    # safe_path checks descendants; also reject an aliased ancestor of the source.
    for ancestor in path.parents:
        if ancestor.is_symlink() or (hasattr(ancestor, "is_junction") and ancestor.is_junction()):
            raise LaunchError(f"source ancestor redirects through link: {ancestor}")
    return path


def metagame_bytes(receipt):
    if not isinstance(receipt, dict) or set(receipt) != {"source", "sha256", "bytes", "account_id"}:
        raise LaunchError("metagame fixture requires exact source, sha256, bytes and account_id fields")
    account = receipt["account_id"]
    if not isinstance(account, str) or re.fullmatch(r"[0-9]{1,20}", account) is None:
        raise LaunchError("metagame fixture account_id must be 1 through 20 decimal digits")
    size = receipt["bytes"]
    if type(size) is not int or not 0 < size <= 16384:
        raise LaunchError("metagame fixture bytes must be above 0 and at most 16384")
    if not isinstance(receipt["sha256"], str) or re.fullmatch(r"[0-9a-f]{64}", receipt["sha256"]) is None:
        raise LaunchError("metagame fixture SHA256 must be lowercase hexadecimal")
    if not isinstance(receipt["source"], str):
        raise LaunchError("metagame fixture source must be a path string")
    supplied = Path(receipt["source"])
    supplied = supplied if supplied.is_absolute() else ROOT / supplied
    source = checked_source(supplied)
    owned = ROOT / "_MPTest"
    if source != supplied.absolute() or source.resolve() != source or not source.is_relative_to(owned):
        raise LaunchError("metagame fixture source must be an unaliased owned repository _MPTest path")
    safe_path(source, owned)
    if source.name != "metagame.vrt" or source.parent.name != account:
        raise LaunchError("metagame fixture source filename/account differs")
    with source.open("rb") as stream:
        data = stream.read(16385)
    if len(data) != size or hashlib.sha256(data).hexdigest() != receipt["sha256"]:
        raise LaunchError("metagame fixture source bytes/hash differs from explicit receipt")
    return source, data


def load_metagame_fixture(path):
    if path is None:
        return None
    receipt = read_json(checked_source(path))
    source, _ = metagame_bytes(receipt)
    return {**receipt, "source": str(source)}


def metagame_target(receipt):
    return "profile/game-data/" + receipt["account_id"] + "/metagame.vrt"


def source_files(source, args, metagame=None):
    actual = sha256(source / "MT2.exe")
    if actual != EXPECTED_EXE_SHA256:
        raise LaunchError(f"unsupported MT2.exe SHA256 actual={actual} required={EXPECTED_EXE_SHA256}; source not copied")
    actual = sha256(source / "steam_api64.dll")
    if actual != args.stock_steam_sha256:
        raise LaunchError(f"stock steam_api64.dll SHA256 actual={actual} required={args.stock_steam_sha256}; source not copied")
    files = {"game/MT2.exe": source / "MT2.exe"}
    excluded = {"wsock32.dll", "mt2framework.dll", "mt2_multiplayer.dll", "steam_api64_original.dll"}
    for path in source.glob("*.dll"):
        if path.name.lower() not in excluded:
            files["game/" + path.name] = path
    data = source / "Data"
    if not data.is_dir():
        raise LaunchError(f"game assets missing: {data}")
    for path in data.rglob("*"):
        safe_path(path, source)
        if path.is_file():
            files["game/" + path.relative_to(source).as_posix()] = path
    if (source / "curl-ca-bundle.crt").is_file():
        files["game/curl-ca-bundle.crt"] = source / "curl-ca-bundle.crt"
    framework = checked_source(args.framework_runtime)
    for name in ("WSOCK32.dll", "MT2Framework.dll"):
        files["game/" + name] = framework / name
    sidecar, binary = checked_source(args.sidecar), checked_source(args.candidate_binary)
    if sidecar.name != "mt2mp_sidecar.exe" or binary.name != "MT2MultiplayerPlugin.dll":
        raise LaunchError("candidate and sidecar require their exact native filenames")
    files["game/mt2mp_sidecar.exe"] = sidecar
    packages = [(args.multiplayer_package, "doodesch.multiplayer", "MT2MultiplayerPlugin.dll", binary)]
    if args.profiler_package:
        packages.append((args.profiler_package, "doodesch.profiler", "MT2Profiler.dll", None))
    for package, identifier, entry, explicit in packages:
        package = checked_source(package)
        manifest = read_json(safe_path(package / "mod.json", package))
        if manifest.get("id") != identifier or manifest.get("type") != "native" or manifest.get("entry") != entry:
            raise LaunchError(f"unexpected native package identity/entry: {package}")
        for path in package.rglob("*"):
            safe_path(path, package)
            if path.is_file():
                relative = path.relative_to(package).as_posix()
                if path.suffix.lower() == ".dll" and relative != entry:
                    raise LaunchError(f"unexpected package DLL: {path}")
                files[f"profile/mods/{identifier}/{relative}"] = path
        files[f"profile/mods/{identifier}/{entry}"] = explicit or package / entry
    for name in legacy.MOD_UI_ASSETS:
        files["game/mt2mp_ui/" + name] = ROOT / "mt2mp_ui" / name
    if metagame is not None:
        fixture_source, _ = metagame_bytes(metagame)
        relative = metagame_target(metagame)
        if relative in files:
            raise LaunchError("metagame fixture target already exists in copy plan")
        files[relative] = fixture_source
    for path in files.values():
        checked_source(path)
        if not path.is_file():
            raise LaunchError(f"required explicit build/package file missing: {path}")
    return files


def prepare_instance(runtime, root, files, metagame=None):
    # A fresh run owns this directory; an explicit fixture is the only imported game-data file.
    safe_path(runtime, root).mkdir(parents=True, exist_ok=False)
    receipt = {}
    for relative, source in files.items():
        destination = safe_path(runtime / relative, root)
        destination.parent.mkdir(parents=True, exist_ok=True)
        if metagame is not None and relative == metagame_target(metagame):
            fixture_source, data = metagame_bytes(metagame)
            if source != fixture_source:
                raise LaunchError("metagame copy plan source differs from explicit receipt")
            before = metagame["sha256"]
            with destination.open("xb") as stream:
                stream.write(data)
        else:
            before = sha256(source)
            shutil.copy2(source, destination)
        if sha256(destination) != before:
            raise LaunchError(f"copied file differs from source receipt: {destination}")
        receipt[relative] = {"source": str(source), "sha256": before}
        if metagame is not None and relative == metagame_target(metagame):
            receipt[relative].update(bytes=metagame["bytes"], account_id=metagame["account_id"])
    for name in ("logs", "profile/game-data", "profile/diagnostics/multiplayer", "profile/data/doodesch.multiplayer"):
        safe_path(runtime / name, root).mkdir(parents=True, exist_ok=True)
    atomic_json(runtime / "copy-manifest.json", receipt)
    atomic_json(runtime / "game/mt2fw.json", {"enabled": True, "profile": str(runtime / "profile")})
    (runtime / "game/steam_appid.txt").write_text(STEAM_APP_ID + "\n", encoding="ascii")


def reject_pending(root, previous):
    if not previous:
        return
    for record in previous["processes"]:
        runtime = Path(record["runtime_root"])
        for relative in ("diagnostics/request.lock", "profile/diagnostics/multiplayer/request.lock"):
            path = safe_path(runtime / relative, root)
            if path.exists():
                raise LaunchError(f"unresolved previous Multiplayer lock retained: {path}; recover with its exact previous manifest before starting")


def local_config(runtime, run_id, peer, session, token, control, transport):
    return {"schema_version": 1, "run_id": run_id, "owner": {"kind": "MPTest", "run_id": run_id},
            "runtime_root": str(runtime), "peer_id": peer, "session_id": session, "control_port": control,
            "session_token": token, "transport": transport}


def game_readiness(status, record, expected_data, expected_account=None):
    if not isinstance(status, dict) or type(status.get("schema_version")) is not int or status.get("schema_version") != 1:
        return "schema_version must be 1"
    if type(status.get("pid")) is not int or type(status.get("peer_id")) is not int:
        return "status pid/peer_id must be integers"
    for name in ("run_id", "pid", "peer_id", "runtime_kind", "framework_profile", "installed_binary_sha256"):
        if status.get(name) != record[name]:
            return f"status {name} differs from launched Framework identity"
    # The SDK supplies the base; the game can select its numeric account child.
    actual = status.get("write_dir")
    if not isinstance(actual, str) or not Path(actual).is_absolute():
        return f"PHYSFS write_dir={actual!r} required=absolute private game-data path"
    actual_path = Path(actual)
    account_child = (legacy.same_path(actual_path.parent, expected_data)
                     and re.fullmatch(r"[0-9]{1,20}", actual_path.name) is not None)
    if (any(part in (".", "..") for part in actual.replace("\\", "/").split("/"))
            or not (legacy.same_path(actual_path, expected_data) or account_child)):
        return f"PHYSFS write_dir={actual!r} required={str(expected_data)!r} or its direct numeric account child"
    if expected_account is not None and not legacy.same_path(actual_path, expected_data / expected_account):
        return f"metagame fixture PHYSFS write_dir={actual!r} required={str(expected_data / expected_account)!r}"
    safe_path(actual_path, Path(record["runtime_root"]))
    if not actual_path.is_dir():
        return f"PHYSFS write_dir does not exist: {actual!r}"
    if status.get("game_thread_ready") is not True or type(status.get("game_thread_id")) is not int or status["game_thread_id"] <= 0:
        return "game thread has not acknowledged readiness"
    if status.get("session_connected") is not True:
        return f"session is not connected: {status.get('session_status', 'unavailable')}"
    return None


def wait_games(games, started, timeout, expected_account=None):
    deadline, previous, failures = time.monotonic() + timeout, {}, {}
    while True:
        check_processes(started)
        waiting = []
        for record in games:
            path = safe_path(Path(record["runtime_root"]) / "profile/diagnostics/multiplayer/status.json", Path(record["runtime_root"]))
            try:
                status = read_status_json(path)
                reason = game_readiness(status, record, Path(record["framework_profile"]) / "game-data", expected_account)
            except (OSError, ValueError) as error:
                reason = f"status acknowledgement unavailable: {error}"
                count, last = failures.get(record["pid"], (0, 0))
                now = time.monotonic()
                if not count or now - last >= 5:
                    log("warn", f"status read peer={record['peer_id']} count={count + 1} path={path}: {error}")
                    last = now
                failures[record["pid"]] = count + 1, last
            if reason:
                waiting.append(f"peer={record['peer_id']} {reason}")
            if previous.get(record["pid"], "unobserved") != reason:
                log("info", f"peer={record['peer_id']} " + (f"waiting: {reason}" if reason else "Framework identity, private saves and session acknowledged"))
                previous[record["pid"]] = reason
        if not waiting:
            for pid, (count, _) in failures.items():
                log("info", f"status read recovered pid={pid} failures={count}")
            return
        if time.monotonic() >= deadline:
            raise LaunchError(f"game readiness timed out required={timeout}s; " + "; ".join(waiting))
        time.sleep(0.1)


def launch(args, processes):
    source, root = checked_source(args.source), Path(os.path.abspath(args.test_root))
    if "_mptest" not in {part.casefold() for part in root.parts}:
        raise LaunchError("Framework test root must be inside _MPTest")
    if root.is_relative_to(source) or source.is_relative_to(root):
        raise LaunchError("source game and test root must be separate directories")
    safe_path(root, root)
    previous = check_inactive(root, processes)
    reject_pending(root, previous)
    metagame = load_metagame_fixture(getattr(args, "metagame_fixture", None))
    files = source_files(source, args, metagame) if metagame is not None else source_files(source, args)
    archive_previous(root, previous)
    run_id = str(uuid.uuid4())
    runtimes = [root / "runs" / run_id / ("host" if peer == 1 else f"client-{peer - 1}") for peer in range(1, args.players + 1)]
    for runtime in runtimes:
        if metagame is not None:
            prepare_instance(runtime, root, files, metagame)
        else:
            prepare_instance(runtime, root, files)
    reservations = reserve_ports([args.relay_port, *range(args.control_port, args.control_port + args.players)])
    session, token = secrets.randbits(64) or 1, uuid.uuid4().hex
    manifest = {"schema_version": 1, "run_id": run_id, "owner": {"kind": "MPTest", "run_id": run_id},
                "test_root": str(root), "source_root": str(source), "session_id": str(session),
                "players": args.players, "relay_port": args.relay_port, "state": "starting", "processes": []}
    if metagame is not None:
        manifest["metagame_fixture"] = {**metagame, "target_relative": metagame_target(metagame), "write_dir_verified": False}
    manifest_path = safe_path(root / "run.json", root)
    atomic_json(manifest_path, manifest)
    started = []

    def start(runtime, role, command, peer=0, env=None):
        installed_binary_sha256 = None
        if role == "game":
            copy_manifest = read_json(safe_path(runtime / "copy-manifest.json", root))
            installed_binary_sha256 = {}
            for name, relative in MODULES.items():
                actual = sha256(safe_path(runtime / relative, root))
                expected = copy_manifest.get(relative, {}).get("sha256")
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
            record.update(schema_version=2, runtime_kind="framework-plugin", framework_profile=str(runtime / "profile"))
            record["installed_binary_sha256"] = installed_binary_sha256
            record["game_log_paths"] = [str(runtime / "profile/data/doodesch.multiplayer/mt2mp.log"), str(runtime / "game/MT2.RPT")]
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
                           "--local-connect-timeout-ms", str(math.ceil(args.ready_timeout * 1000)),
                           "--session", str(session), "--token", token], peer)
            wait_marker(Path(bridge["log_path"]), "CONTROL_READY", started, args.ready_timeout)
            atomic_json(safe_path(runtime / "profile/data/doodesch.multiplayer/local-test.json", root),
                        local_config(runtime, run_id, peer, session, token, control, args.transport))
            env = {key: value for key, value in os.environ.items() if not key.upper().startswith("MT2MP_")}
            env.update(SteamAppId=STEAM_APP_ID, SteamGameId=STEAM_APP_ID)
            games.append(start(runtime, "game", [str(runtime / "game" / "MT2.exe")], peer, env))
        if metagame is not None:
            wait_games(games, started, args.ready_timeout, metagame["account_id"])
            manifest["metagame_fixture"]["write_dir_verified"] = True
        else:
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
    result.add_argument("--stop", action="store_true", help="Stop only exact owned process identities in this root")
    result.add_argument("--players", type=int, choices=(2,), default=2)
    result.add_argument("--source", type=Path, default=DEFAULT_SOURCE)
    result.add_argument("--test-root", type=Path, default=ROOT / "_MPTest/framework-local")
    result.add_argument("--candidate-binary", type=Path)
    result.add_argument("--multiplayer-package", type=Path)
    result.add_argument("--framework-runtime", type=Path)
    result.add_argument("--sidecar", type=Path)
    result.add_argument("--profiler-package", type=Path)
    result.add_argument("--metagame-fixture", type=Path, help="Explicit owned _MPTest metagame.vrt receipt; never imports a whole profile")
    result.add_argument("--stock-steam-sha256")
    result.add_argument("--transport", choices=("loopback", "steam"), default="loopback")
    result.add_argument("--relay-port", type=int, default=47010)
    result.add_argument("--control-port", type=int, default=47001)
    result.add_argument("--ready-timeout", type=float, default=60)
    return result


def main(argv=None):
    args = parser().parse_args(argv)
    if not args.stop and (not all((args.candidate_binary, args.multiplayer_package, args.framework_runtime, args.sidecar))
                          or not re.fullmatch(r"[0-9a-f]{64}", args.stock_steam_sha256 or "")):
        log("error", "launch requires explicit candidate binary, Multiplayer package, Framework runtime, sidecar and stock Steam SHA256")
        return 2
    if not (0 < args.ready_timeout <= 4294967.295) or not (1 <= args.relay_port <= 65535) or not (1 <= args.control_port <= 65534):
        log("error", "invalid timeout or port range")
        return 2
    if args.relay_port in range(args.control_port, args.control_port + args.players):
        log("error", "relay port overlaps a control port")
        return 2
    try:
        processes = WindowsProcesses()
        with RuntimeLock(args.test_root):
            if args.stop:
                return stop_manifest(Path(os.path.abspath(args.test_root)), processes)
            return launch(args, processes)
    except (LaunchError, OSError, ValueError, KeyError, TypeError) as error:
        log("error", str(error))
        return 1


if __name__ == "__main__":
    sys.exit(main())
