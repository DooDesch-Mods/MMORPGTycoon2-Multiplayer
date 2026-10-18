"""Build isolated snapshot fixtures and stream an actual generated file over loopback."""
from __future__ import annotations
import argparse
import ctypes
from ctypes import wintypes
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import socket
import struct
import subprocess
import threading
import time
import uuid

ROOT = Path(__file__).resolve().parents[1]
MAGIC = 0x4D32524C


def frame(kind, channel, payload=b""):
    return struct.pack("<IBBHI", MAGIC, kind, channel, 1, len(payload)) + payload


def hash_file(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for data in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(data)
    return digest.hexdigest()


class Relay:
    def __init__(self, mode, players):
        self.mode = mode
        self.players = players
        self.listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        if hasattr(socket, "SO_EXCLUSIVEADDRUSE"):
            self.listener.setsockopt(socket.SOL_SOCKET, socket.SO_EXCLUSIVEADDRUSE, 1)
        self.listener.bind(("127.0.0.1", 0))
        self.port = self.listener.getsockname()[1]
        self.listener.listen(players)
        self.listener.settimeout(0.1)
        self.lock = threading.Lock()
        self.peers = {}
        self.threads = []
        self.stop = threading.Event()
        self.errors, self.events, self.resets = [], [], []
        self.sent_chunks = self.acknowledged = self.peak_unacknowledged = 0
        self.peer_acknowledgements = {peer: 0 for peer in range(2, players + 1)}
        self.peer_verified_at = {}
        self.changed = False
        self.original_manifest = None
        self.accept_thread = threading.Thread(target=self.accept, name="snapshot-relay-accept")
        self.accept_thread.start()

    def accept(self):
        try:
            while not self.stop.is_set():
                try:
                    endpoint = self.listener.accept()[0]
                except socket.timeout:
                    continue
                endpoint.settimeout(0.1)
                thread = threading.Thread(target=self.endpoint, args=(endpoint,), name="snapshot-relay-peer")
                self.threads.append(thread)
                thread.start()
        except OSError as error:
            if not self.stop.is_set():
                self.errors.append(f"relay accept: {error}")
            else:
                self.events.append(f"expected accept cleanup: {error}")

    def close_peers(self, reason):
        with self.lock:
            endpoints = list(self.peers.values())
        self.events.append(reason)
        for endpoint, lock in endpoints:
            try:
                endpoint.shutdown(socket.SHUT_RDWR)
            except OSError as error:
                self.events.append(f"expected peer shutdown {reason}: {error}")

    def send(self, peer, packet):
        with self.lock:
            entry = self.peers.get(peer)
        if entry is None:
            return
        endpoint, lock = entry
        with lock:
            try:
                endpoint.sendall(packet)
            except OSError as error:
                if getattr(error, "winerror", None) == 10038 and endpoint.fileno() == -1:
                    self.events.append(f"expected relay send skipped after owned fixture socket closed target={peer} fileno=-1 cause={error}")
                    return
                raise

    def endpoint(self, endpoint):
        peer = None
        try:
            with endpoint:
                receive = b""
                while not self.stop.is_set():
                    try:
                        data = endpoint.recv(1024 * 1024)
                    except socket.timeout:
                        continue
                    if not data:
                        self.events.append(f"peer TCP EOF peer={peer}")
                        if peer == 1:
                            self.close_peers("host fixture socket closed")
                        return
                    receive += data
                    while len(receive) >= 12:
                        magic, kind, channel, version, length = struct.unpack_from("<IBBHI", receive)
                        if magic != MAGIC or version != 1 or length > 131072:
                            raise ValueError("invalid relay input frame")
                        if len(receive) < 12 + length:
                            break
                        payload, receive = receive[12:12 + length], receive[12 + length:]
                        if kind == 1:
                            session, peer, host = struct.unpack_from("<QQB", payload)
                            if session != 500 or peer not in range(1, self.players + 1) or host != (peer == 1) or payload[17:] != b"b" * 32:
                                raise ValueError("snapshot fixture identity invalid")
                            with self.lock:
                                if peer in self.peers:
                                    raise ValueError("duplicate fixture peer")
                                self.peers[peer] = (endpoint, threading.Lock())
                            self.send(peer, frame(3, 0))
                        elif kind == 2:
                            if peer is None or len(payload) < 16:
                                raise ValueError("data before fixture authentication")
                            target, _ = struct.unpack_from("<QQ", payload)
                            body = payload[16:]
                            if peer == 1 and channel == 4 and json.loads(body).get('type') == 'snapshot_begin':
                                self.original_manifest = body
                            if peer != 1 and channel == 4:
                                control = json.loads(body)
                                if control['type'] == 'fixture_retired_ready':
                                    if self.mode != 'retired_replay' or self.original_manifest is None:
                                        raise ValueError('unexpected retired fixture handshake')
                                    self.events.append('replay original manifest after explicit client world retirement')
                                    self.send(peer, frame(2, 4, struct.pack('<QQ', peer, 1) + self.original_manifest))
                                    continue
                                if control["type"] == "snapshot_chunk_ack":
                                    self.peer_acknowledgements[peer] = control["next"]
                                    self.acknowledged = min(self.peer_acknowledgements.values())
                                elif control["type"] == "snapshot_verified":
                                    self.peer_verified_at[peer] = time.monotonic()
                            if peer == 1 and channel == 5:
                                self.sent_chunks += 1
                                self.peak_unacknowledged = max(self.peak_unacknowledged, self.sent_chunks - self.acknowledged)
                                if self.mode == "data_idle":
                                    if not self.changed:
                                        self.changed = True
                                        self.events.append("deliberate data silence; transport heartbeat remains alive")
                                    continue
                                if not self.changed:
                                    self.changed = True
                                    if self.mode == "corrupt":
                                        body = body[:48] + bytes([body[48] ^ 1]) + body[49:]
                                        self.events.append("deliberate chunk corruption")
                                    elif self.mode == "order":
                                        body = body[:36] + struct.pack("<I", 1) + body[40:]
                                        self.events.append("deliberate chunk index disorder")
                                    elif self.mode == "eof":
                                        self.close_peers("deliberate partial-transfer TCP EOF")
                                        return
                            routed = frame(2, channel, struct.pack("<QQ", target, peer) + body)
                            targets = [destination for destination in range(1, self.players + 1) if destination != peer] if target == 0 else [target]
                            for destination in targets:
                                if destination != peer:
                                    self.send(destination, routed)
                                    if self.mode == "replay" and peer == 1 and channel == 5 and self.sent_chunks == 1:
                                        self.events.append("deliberate identical chunk replay")
                                        self.send(destination, routed)
                        else:
                            raise ValueError(f"unexpected fixture relay type={kind}")
        except ConnectionResetError as error:
            self.resets.append({"peer": peer, "cause": str(error)})
        except OSError as error:
            if self.stop.is_set() or any("socket closed" in event or "TCP EOF" in event for event in self.events):
                self.events.append(f"expected terminal peer close peer={peer}: {error}")
            else:
                self.errors.append(f"peer={peer} socket error: {error}")
        except Exception as error:
            self.errors.append(f"peer={peer} {type(error).__name__}: {error}")

    def finish(self):
        self.stop.set()
        self.close_peers("owner fixture cleanup")
        self.listener.close()
        self.accept_thread.join(5)
        for thread in self.threads:
            thread.join(5)
        if self.accept_thread.is_alive() or any(thread.is_alive() for thread in self.threads):
            raise RuntimeError("snapshot relay cleanup exceeded 5s")


def build(directory):
    vswhere = Path("C:/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe")
    found = subprocess.run([str(vswhere), "-latest", "-products", "*", "-requires",
                            "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath"],
                           capture_output=True, text=True, timeout=10, check=False)
    if found.returncode or not found.stdout.strip():
        raise RuntimeError(f"MSVC discovery exit={found.returncode} cause={found.stderr}")
    vcvars = Path(found.stdout.strip()) / "VC/Auxiliary/Build/vcvars64.bat"
    common = ["cl", "/nologo", "/std:c++20", "/EHsc", "/utf-8", "/DWIN32_LEAN_AND_MEAN", "/DNOMINMAX",
              "/I" + str(ROOT / "include"), "/I" + str(ROOT / "include/vendor/steam"),
              "/Fo" + directory.as_posix() + "/", "/DMT2MP_SNAPSHOT_TEST"]
    commands = [
        [option for option in common if option != "/DNOMINMAX"] + ["/Fe" + str(directory / "file_guards.exe"), str(ROOT / "test/test_snapshot_buffer.cpp")],
        common + ["/Fe" + str(directory / "load_deadline_guards.exe"), str(ROOT / "test/snapshot_load_deadline_fixture.cpp")],
        common + ["/LD", "/Fe" + str(directory / "libphysfs.dll"), str(ROOT / "test/snapshot_stream_physfs_fixture.cpp")],
        common + ["/Fe" + str(directory / "snapshot_stream_fixture.exe"), str(ROOT / "test/snapshot_stream_fixture.cpp"),
                  str(ROOT / "src/net/sidecar_client.cpp"), str(ROOT / "src/net/local_session.cpp"),
                  str(ROOT / "src/net/snapshot_sync.cpp"), "/link", "ws2_32.lib", "psapi.lib"],
    ]
    script = directory / "build.cmd"
    script.write_text('@echo off\ncall "' + str(vcvars) + '"\nif errorlevel 1 exit /b %errorlevel%\n' +
                      "\n".join(subprocess.list2cmdline(command) + "\nif errorlevel 1 exit /b %errorlevel%" for command in commands) +
                      "\nexit /b 0\n", encoding="utf-8")
    with (directory / "compile.log").open("w", encoding="utf-8") as log:
        result = subprocess.run(["cmd.exe", "/d", "/c", str(script)], cwd=directory, stdout=log,
                                stderr=subprocess.STDOUT, timeout=60, check=False)
    if result.returncode:
        raise RuntimeError(f"fixture compile exit={result.returncode} log={directory / 'compile.log'}")


def transfer(directory, source, digest, mode, budget, players, network_map):
    scenario = directory / mode
    scenario.mkdir()
    relay = Relay(mode, players)
    processes, streams = [], []
    started = time.monotonic()
    try:
        roles = [(peer, "client" if peer == 2 else f"client-{peer}") for peer in range(2, players + 1)] + [(1, "host")]
        for peer, role in roles:
            log = (scenario / f"{role}.log").open("w", encoding="utf-8")
            streams.append(log)
            arguments = [str(directory / "snapshot_stream_fixture.exe"), str(peer), str(relay.port),
                         str(scenario / role), str(source), digest, mode, str(budget), str(players)]
            fixture_env = dict(os.environ, MT2MP_FIXTURE_NETWORK_MAP=network_map)
            processes.append((role, subprocess.Popen(arguments, cwd=directory, stdout=log, stderr=subprocess.STDOUT, env=fixture_env)))
        last_progress = last_report = started
        confirmed = 0
        while any(process.poll() is None for _, process in processes):
            now = time.monotonic()
            if relay.acknowledged > confirmed:
                confirmed = relay.acknowledged
                last_progress = now
            if now - started > budget:
                raise RuntimeError(f"fixture total timeout mode={mode} acknowledged_chunks={confirmed} required<={budget}s")
            if len(relay.peer_verified_at) != players - 1 and now - last_progress > 60:
                raise RuntimeError(f"fixture snapshot progress timeout mode={mode} acknowledged_chunks={confirmed} required idle<=60s")
            if now - last_report >= 20:
                print(f"info: snapshot mode={mode} acknowledged_chunks={confirmed} bytes<={min(source.stat().st_size, confirmed * 48 * 1024)} elapsed={now - started:.1f}s", flush=True)
                last_report = now
            for role, process in processes:
                if process.poll() is not None and process.returncode != 0:
                    raise RuntimeError(f"fixture failed mode={mode} role={role} exit={process.returncode} log={scenario / (role + '.log')}")
                if process.poll() is None:
                    try:
                        process.wait(timeout=0.5)
                    except subprocess.TimeoutExpired:
                        continue
    finally:
        for role, process in processes:
            if process.poll() is None:
                print(f"warn: owned fixture cleanup terminates role={role} pid={process.pid}", flush=True)
                process.terminate()
                process.wait(timeout=5)
        relay.finish()
        for stream in streams:
            stream.close()
    receipts = {}
    for role, process in processes:
        text = (scenario / f"{role}.log").read_text(encoding="utf-8")
        results = [json.loads(line) for line in text.splitlines() if line.startswith('{"') and '"passed"' in line]
        if process.returncode or len(results) != 1 or not results[0].get("passed"):
            raise RuntimeError(f"fixture mode={mode} role={role} exit={process.returncode} log={scenario / (role + '.log')}")
        if relay.resets and "checkpoint: explicit-owner-shutdown" not in text:
            raise RuntimeError(f"unexpected unconfirmed fixture reset mode={mode}: {relay.resets}")
        receipts[role] = {**results[0], "exit_code": process.returncode, "log_path": str(scenario / f"{role}.log")}
    if relay.errors:
        raise RuntimeError(f"fixture relay failed mode={mode} causes={relay.errors}")
    if relay.peak_unacknowledged > 8:
        raise RuntimeError(f"snapshot window exceeded actual={relay.peak_unacknowledged} required<=8")
    target = scenario / "client/data/save/MPTestSession/snapshot-50000000-0000-4000-8000-000000000001.vrz"
    targets = [scenario / role / "data/save/MPTestSession/snapshot-50000000-0000-4000-8000-000000000001.vrz"
               for _, role in roles if role != "host"]
    if mode in ("valid", "load_slow", "load_timeout", "host_load_timeout", "retired_replay"):
        for received in targets:
            if received.stat().st_size != source.stat().st_size or hash_file(received) != digest:
                raise RuntimeError(f"committed file differs from independent source SHA256 target={received}")
    elif target.exists():
        raise RuntimeError(f"failed transfer accepted final file mode={mode}")
    if mode in ("load_timeout", "host_load_timeout"):
        timed_out_role="host" if mode=="host_load_timeout" else "client"
        if "snapshot native load observation timeout elapsed_ms=901" not in receipts[timed_out_role]["status"]:
            raise RuntimeError(f"native deadline cause lost: {receipts[timed_out_role]['status']}")
        if receipts["host"]["snapshot_progress"]["peers_loaded"] != 0 or receipts["client"]["native_load_calls"] != 1:
            raise RuntimeError("native timeout falsely confirmed load or replayed mutation")
    elif mode == "retired_replay":
        if receipts['client']['native_load_calls'] != 1 or 'completed snapshot belongs to retired world' not in receipts['client']['status']:
            raise RuntimeError('retired snapshot replay accepted old map or repeated native load')
        if receipts['client']['snapshot_progress']['world_authority_valid'] is not False:
            raise RuntimeError('retired snapshot restored world authority')
    elif mode == "data_idle":
        host_cause = receipts['host']['status']
        host_deadline = 'snapshot acknowledgement progress timeout' in host_cause
        client_deadline = 'peer=2 snapshot rejected: snapshot receive progress timeout bytes=0 required idle<=60s' in host_cause
        if not (host_deadline or client_deadline):
            raise RuntimeError(f"data deadline cause lost: {receipts['host']['status']}")
        if client_deadline and 'snapshot receive progress timeout bytes=0 required idle<=60s' not in receipts['client']['status']:
            raise RuntimeError('forwarded client deadline differs from original cause')
        if any(receipt["native_load_calls"] for receipt in receipts.values()):
            raise RuntimeError("data timeout invoked native load")
    elif mode not in ("valid", "load_slow"):
        required = {"corrupt": "snapshot SHA256 mismatch", "order": "snapshot chunk order actual=1 required=0",
                    "replay": "snapshot chunk order actual=0 required=1", "eof": "snapshot transport ended: relay TCP EOF"}[mode]
        if required not in receipts["client"]["status"]:
            raise RuntimeError(f"first rejection cause lost mode={mode} actual={receipts['client']['status']} required={required}")
        if mode != "eof" and required not in receipts["host"]["status"]:
            raise RuntimeError(f"peer rejection cause lost mode={mode} actual={receipts['host']['status']} required={required}")
    return {"mode": mode, "passed": True, "elapsed_seconds": time.monotonic() - started, "source_bytes": source.stat().st_size,
            "source_sha256": digest, "peak_unacknowledged_chunks": relay.peak_unacknowledged, "relay_events": relay.events,
            "relay_resets": relay.resets, "peer_receipts": receipts, "target": str(target), "targets": [str(path) for path in targets], "players": players}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--size-mib", type=int, default=500)
    parser.add_argument("--mode", choices=["all", "valid", "corrupt", "order", "replay", "eof", "load_slow", "load_timeout", "host_load_timeout", "data_idle", "retired_replay"], default="all")
    parser.add_argument("--keep-files", action="store_true")
    parser.add_argument("--timeout-seconds", type=int, default=600)
    parser.add_argument("--players", type=int, choices=[2, 3, 4], default=2)
    parser.add_argument("--network-map", choices=["confirmed", "fileonly", "armfail", "digestwrong"], default="confirmed")
    args = parser.parse_args()
    if not 1 <= args.size_mib <= 1024:
        parser.error("--size-mib must be 1..1024")
    if not 1 <= args.timeout_seconds <= 600:
        parser.error("--timeout-seconds must be 1..600; data idle stays bounded to 60s, native load uses a separate 900s deadline")
    if args.players > 2 and args.mode != "valid":
        parser.error("multi-peer fixture uses --mode valid; deliberate rejection cases use two peers")
    directory = ROOT / "test/snapshot_stream_evidence" / str(uuid.uuid4())
    directory.mkdir(parents=True)
    summary = {"scope": "actual file and loopback transfer with production snapshot/session/transport; native load stubs; no game",
               "started_utc": datetime.now(timezone.utc).isoformat(), "directory": str(directory), "size_mib": args.size_mib,
               "checks": [], "passed": False, "cleanup": [], "total_budget_seconds": args.timeout_seconds, "players": args.players, "network_map": args.network_map}
    (directory / "summary.json").write_text(json.dumps(summary, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    try:
        native_sources = ['src/net/snapshot_sync.cpp', 'src/net/sidecar_client.cpp', 'src/net/local_session.cpp',
                          'include/mt2mp/net/snapshot_sync.h', 'include/mt2mp/net/network_identity_metadata.h',
                          'src/game/network_identity.h', 'test/snapshot_stream_fixture.cpp']
        summary['source_sha256'] = {path: hash_file(ROOT / path) for path in native_sources}
        build(directory)
        if any(hash_file(ROOT / path) != digest for path, digest in summary['source_sha256'].items()):
            raise RuntimeError('native fixture sources changed while compiling; candidate unconfirmed')
        summary["fixture_sha256"] = hash_file(directory / "snapshot_stream_fixture.exe")
        with (directory / "file_guards.log").open("w", encoding="utf-8") as log:
            guards = subprocess.run([str(directory / "file_guards.exe"), str(directory / "file_guard_cases")],
                                    cwd=directory, stdout=log, stderr=subprocess.STDOUT, timeout=10, check=False)
        if guards.returncode:
            raise RuntimeError(f"file guards exit={guards.returncode} log={directory / 'file_guards.log'}")
        summary["file_guards_exit"] = guards.returncode
        with (directory / "load_deadline_guards.log").open("w", encoding="utf-8") as log:
            deadlines = subprocess.run([str(directory / "load_deadline_guards.exe")], cwd=directory, stdout=log, stderr=subprocess.STDOUT, timeout=10, check=False)
        if deadlines.returncode:
            raise RuntimeError(f"load deadline guards exit={deadlines.returncode} log={directory / 'load_deadline_guards.log'}")
        summary["load_deadline_guards_exit"] = deadlines.returncode
        source = directory / "generated.vrz"
        block = bytes((index * 17 + 13) % 256 for index in range(1024 * 1024))
        digest = hashlib.sha256()
        with source.open("xb") as stream:
            for _ in range(args.size_mib):
                stream.write(block)
                digest.update(block)
            stream.flush()
            import os
            os.fsync(stream.fileno())
        del block
        source_digest = digest.hexdigest()
        summary["source_sha256"] = source_digest
        for mode in (["valid", "corrupt", "order", "replay", "eof", "load_slow", "load_timeout", "host_load_timeout", "data_idle", "retired_replay"] if args.mode == "all" else [args.mode]):
            actual_source = source
            actual_digest = source_digest
            if mode != "valid":
                actual_source = directory / "small.vrz"
                if not actual_source.exists():
                    with source.open("rb") as original, actual_source.open("xb") as small:
                        small.write(original.read(2 * 48 * 1024 + 3))
                actual_digest = hash_file(actual_source)
            print(f"info: snapshot fixture mode={mode} bytes={actual_source.stat().st_size}", flush=True)
            summary["current_mode"] = mode
            (directory / "summary.json").write_text(json.dumps(summary, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
            summary["checks"].append(transfer(directory, actual_source, actual_digest, mode, args.timeout_seconds, args.players, args.network_map))
            (directory / "summary.json").write_text(json.dumps(summary, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        if not args.keep_files:
            for path in [source, *(Path(target) for check in summary["checks"] if check["mode"] in ("valid", "load_slow") for target in check["targets"])]:
                resolved = path.resolve()
                if not resolved.is_relative_to(directory.resolve()):
                    raise RuntimeError(f"cleanup path outside own fixture root: {resolved}")
                if path.exists():
                    path.unlink()
                    summary["cleanup"].append({"path": str(path), "state": "own generated file removed after SHA256 confirmation"})
        summary["source_sha256_files"] = {str(path.relative_to(ROOT)): hash_file(path) for path in
            [ROOT / "include/mt2mp/net/snapshot_buffer.h", ROOT / "include/mt2mp/net/snapshot_sync.h",
             ROOT / "src/net/snapshot_sync.cpp", ROOT / "src/net/sidecar_client.cpp", ROOT / "src/net/local_session.cpp",
             ROOT / "test/snapshot_stream_fixture.cpp", ROOT / "test/snapshot_load_deadline_fixture.cpp", ROOT / "test/snapshot_stream_test.py"]}
        summary["passed"] = True
        class Counters(ctypes.Structure):
            _fields_ = [("cb", wintypes.DWORD), ("page_faults", wintypes.DWORD)] + [(name, ctypes.c_size_t) for name in
                       ["peak_working_set", "working_set", "quota_peak_paged", "quota_paged", "quota_peak_nonpaged",
                        "quota_nonpaged", "pagefile", "peak_pagefile", "private_usage"]]
        counters = Counters()
        counters.cb = ctypes.sizeof(counters)
        kernel = ctypes.WinDLL("kernel32", use_last_error=True)
        kernel.GetCurrentProcess.restype = wintypes.HANDLE
        api = ctypes.WinDLL("psapi", use_last_error=True).GetProcessMemoryInfo
        api.argtypes = [wintypes.HANDLE, ctypes.POINTER(Counters), wintypes.DWORD]
        if not api(kernel.GetCurrentProcess(), ctypes.byref(counters), counters.cb):
            raise OSError(ctypes.get_last_error(), "fixture runner memory measurement failed")
        summary["runner_memory"] = {"peak_working_set_bytes": counters.peak_working_set, "private_usage_bytes": counters.private_usage}
    except Exception as error:
        summary["passed"] = False
        summary["error"] = {"operation": "isolated snapshot fixture", "cause": f"{type(error).__name__}: {error}"}
        print(f"error: {summary['error']['cause']} evidence={directory}", flush=True)
    finally:
        (directory / "summary.json").write_text(json.dumps(summary, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"passed": summary["passed"], "summary": str(directory / "summary.json")}), flush=True)
    return 0 if summary["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
