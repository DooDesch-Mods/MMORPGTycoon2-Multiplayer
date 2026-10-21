"""Build and run isolated transport fixtures; never start or stop a game."""
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import shutil
import socket
import struct
import subprocess
import sys
import threading
import time
import unittest

ROOT = Path(__file__).resolve().parents[1]
EVIDENCE = ROOT / "test/session_liveness_evidence"
MAGIC = 0x4D32524C


def frame(kind, channel, payload=b""):
    return struct.pack("<IBBHI", MAGIC, kind, channel, 1, len(payload)) + payload


class RelayFixture:
    def __init__(self, mode):
        self.mode = mode
        self.initial_mode = mode
        self.listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        if hasattr(socket, "SO_EXCLUSIVEADDRUSE"):
            self.listener.setsockopt(socket.SOL_SOCKET, socket.SO_EXCLUSIVEADDRUSE, 1)
        self.listener.bind(("127.0.0.1", 0))
        self.port = self.listener.getsockname()[1]
        self.listener.listen(1)
        self.listener.settimeout(5)
        self.stop = threading.Event()
        self.errors, self.events = [], []
        self.terminal_reset = None
        self.heartbeats, self.sequence = 0, 0
        self.client_instance = None
        self.generation = 1
        self.roster_sent = False
        self.thread = threading.Thread(target=self.run, name=f"session-relay-{mode}")
        self.thread.start()

    def control(self, kind, sequence=None):
        self.sequence += 1
        payload = json.dumps({"version": 2, "session": 42, "sequence": self.sequence if sequence is None else sequence,
                              "instance": 101, "participating": True, "type": kind,
                              "generation": self.generation,
                              "slots": [0, 101, self.client_instance, 303 if self.generation == 2 else 0, 0]}).encode()
        return frame(2, 3, struct.pack("<QQ", 2, 1) + payload)

    def run(self):
        try:
            with self.listener.accept()[0] as endpoint:
                endpoint.settimeout(0.05)
                receive = b""
                authenticated, welcomed = False, False
                connected_time = None
                last_heartbeat = 0
                while not self.stop.is_set():
                    try:
                        chunk = endpoint.recv(131072)
                        if not chunk:
                            self.events.append("confirmed-client-socket-close")
                            break
                        receive += chunk
                    except socket.timeout:
                        chunk = None
                    except ConnectionResetError as error:
                        if self.initial_mode != "overflow" or "deliberate-mainthread-queue-overflow" not in self.events:
                            raise
                        self.events.append(f"observed-overflow-peer-reset: {error}")
                        print(f"info: overflow fixture peer reset after deliberate queue fill: {error}", flush=True)
                        break
                    while len(receive) >= 12:
                        magic, kind, channel, version, length = struct.unpack_from("<IBBHI", receive)
                        if magic != MAGIC or version != 1 or length > 131072:
                            raise ValueError("invalid fixture input frame")
                        if len(receive) < 12 + length:
                            break
                        payload, receive = receive[12:12 + length], receive[12 + length:]
                        if kind == 1:
                            session, peer, host = struct.unpack_from("<QQB", payload)
                            if (session, peer, host) != (42, 2, 0):
                                raise ValueError("fixture transport identity mismatch")
                            endpoint.sendall(frame(3, 0))
                            authenticated = True
                        elif kind == 2 and channel == 3:
                            message = json.loads(payload[16:])
                            if message["type"] == "hello" and not welcomed:
                                if message["version"] != 2 or not message["instance"] or not message["participating"]:
                                    raise ValueError("fixture process membership handshake mismatch")
                                self.client_instance = message["instance"]
                                endpoint.sendall(self.control("welcome"))
                                endpoint.sendall(frame(2, 4, struct.pack("<QQ", 2, 1) + b"snapshot-once"))
                                endpoint.sendall(frame(2, 6, struct.pack("<QQ", 2, 1) + b"action-once"))
                                welcomed = True
                                connected_time = time.monotonic()
                            elif message["type"] == "heartbeat":
                                self.heartbeats += 1
                    now = time.monotonic()
                    if authenticated and welcomed:
                        if self.mode == "roster" and not self.roster_sent and now - connected_time >= 0.25:
                            self.generation = 2
                            endpoint.sendall(self.control("binding"))
                            endpoint.sendall(frame(2, 14, struct.pack("<QQ", 2, 1) + b"roster-queued"))
                            self.roster_sent = True
                        if self.mode in ("stall", "roster") and now - last_heartbeat >= 0.2:
                            endpoint.sendall(self.control("heartbeat"))
                            last_heartbeat = now
                        elif self.mode == "replay" and now - last_heartbeat >= 0.2:
                            endpoint.sendall(self.control("heartbeat", sequence=1))
                            last_heartbeat = now
                        elif self.mode == "eof" and now - connected_time >= 0.25:
                            self.events.append("deliberate-relay-TCP-EOF")
                            break
                        elif self.mode == "overflow" and now - connected_time >= 0.2:
                            self.events.append("deliberate-mainthread-queue-overflow")
                            endpoint.settimeout(3)
                            for _ in range(130):
                                try:
                                    endpoint.sendall(frame(2, 4, struct.pack("<QQ", 2, 1) + b"x" * 65536))
                                except (ConnectionResetError, BrokenPipeError) as error:
                                    self.events.append(f"observed-overflow-send-close: {error}")
                                    print(f"info: overflow fixture send stopped after deliberate queue fill: {error}", flush=True)
                                    break
                            self.mode = "overflow-sent"
        except ConnectionResetError as error:
            self.terminal_reset = f"{type(error).__name__}: {error}"
            print(f"warn: relay fixture {self.mode} reset requires result validation: {error}", flush=True)
        except Exception as error:
            self.errors.append(f"{type(error).__name__}: {error}")
            print(f"error: relay fixture {self.mode}: {error}", flush=True)
        finally:
            self.listener.close()

    def finish(self):
        self.stop.set()
        self.thread.join(timeout=5)
        if self.thread.is_alive():
            raise RuntimeError(f"fixture relay did not stop mode={self.mode} required=5s")


class SessionLivenessTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        EVIDENCE.mkdir(parents=True, exist_ok=True)
        stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")
        previous_checks = EVIDENCE / "checks.json"
        if previous_checks.exists():
            previous_run = EVIDENCE / ("previous_run_" + stamp)
            previous_run.mkdir()
            for previous in [previous_checks, *EVIDENCE.glob("*.log")]:
                shutil.copy2(previous, previous_run / previous.name)
        cls.executable = EVIDENCE / "session_liveness_fixture.exe"
        previous_log = EVIDENCE / "compile.log"
        if previous_log.exists() and previous_log.stat().st_size:
            stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")
            shutil.copy2(previous_log, EVIDENCE / f"previous_compile_{stamp}.log")
        vswhere = Path(r"C:/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe")
        location = subprocess.run([str(vswhere), "-latest", "-products", "*", "-requires", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath"],
                                  capture_output=True, text=True, timeout=10, check=False)
        if location.returncode or not location.stdout.strip():
            raise RuntimeError(f"MSVC discovery failed exit={location.returncode} cause={location.stderr}")
        vcvars = Path(location.stdout.strip()) / "VC/Auxiliary/Build/vcvars64.bat"
        if not vcvars.is_file():
            raise RuntimeError(f"MSVC environment script missing: {vcvars}")
        options = ["cl", "/nologo", "/std:c++20", "/EHsc", "/utf-8", "/DWIN32_LEAN_AND_MEAN", "/DNOMINMAX", "/I", str(ROOT / "include"),
                   "/I", str(ROOT / "include/vendor/steam"), "/Fo" + EVIDENCE.as_posix() + "/", "/Fe" + str(cls.executable),
                   str(ROOT / "test/session_liveness_fixture.cpp"), str(ROOT / "src/net/sidecar_client.cpp"), str(ROOT / "src/net/local_session.cpp"), "/link", "ws2_32.lib", "bcrypt.lib"]
        build_script = ROOT / "test/session_liveness_build.cmd"
        build_script.write_text('@echo off\ncall "' + str(vcvars) + '"\nif errorlevel 1 exit /b %errorlevel%\n' + subprocess.list2cmdline(options) + '\nexit /b %errorlevel%\n', encoding="utf-8")
        command = ["cmd.exe", "/d", "/c", str(build_script)]
        with (EVIDENCE / "compile.log").open("w", encoding="utf-8") as stream:
            result = subprocess.run(command, cwd=ROOT, stdout=stream, stderr=subprocess.STDOUT, timeout=60, check=False)
        if result.returncode:
            raise RuntimeError(f"fixture compilation exit={result.returncode}: {(EVIDENCE / 'compile.log').read_text(encoding='utf-8')[-6000:]}")
        cls.receipts = {"scope": "real loopback sockets and production transport/session code, no game process", "started_utc": datetime.now(timezone.utc).isoformat(),
                        "compile_exit": result.returncode, "fixture_sha256": hashlib.sha256(cls.executable.read_bytes()).hexdigest(),
                        "source_sha256": {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest()
                                          for path in [ROOT / "src/net/sidecar_client.cpp", ROOT / "src/net/local_session.cpp",
                                                       ROOT / "include/mt2mp/net/sidecar_client.h", ROOT / "include/mt2mp/net/local_session.h",
                                                       ROOT / "include/mt2mp/net/local_membership.h", ROOT / "include/mt2mp/net/session_binding.h"]}, "checks": []}

    def check_mode(self, mode):
        relay = RelayFixture(mode)
        log_path = EVIDENCE / f"{mode}.log"
        try:
            with log_path.open("w", encoding="utf-8") as stream:
                result = subprocess.run([str(self.executable), str(relay.port), mode], cwd=ROOT,
                                        stdout=stream, stderr=subprocess.STDOUT, timeout=25, check=False)
        finally:
            relay.finish()
        text = log_path.read_text(encoding="utf-8")
        if relay.terminal_reset:
            orderly_fixture_end = (mode in ("stall", "roster") and result.returncode == 0 and '"passed":true' in text
                                   and "checkpoint: explicit-owner-shutdown" in text
                                   and "TRANSPORT_WORKER_STOPPED" in text and "SESSION_ENDED" not in text)
            if orderly_fixture_end:
                relay.events.append("confirmed-explicit-owner-shutdown-reset: " + relay.terminal_reset)
                print("info: fixture reset confirmed after explicit owner shutdown", flush=True)
            else:
                relay.errors.append(relay.terminal_reset)
        record = {"mode": mode, "exit_code": result.returncode, "log_path": str(log_path), "received_client_heartbeats": relay.heartbeats,
                  "fixture_errors": relay.errors, "events": relay.events, "passed": result.returncode == 0 and not relay.errors}
        if mode == "overflow":
            record["queue_limit_confirmed"] = "mainthread receive queue limit bytes=8388632 required<=8388608" in text
            record["passed"] = record["passed"] and record["queue_limit_confirmed"]
        self.receipts["checks"].append(record)
        (EVIDENCE / "checks.json").write_text(json.dumps(self.receipts, indent=2) + "\n", encoding="utf-8")
        self.assertEqual(result.returncode, 0, text)
        self.assertEqual(relay.errors, [], text)
        self.assertIn('"passed":true', text)
        if mode == "overflow":
            self.assertTrue(record["queue_limit_confirmed"], text)
        if mode == "stall":
            self.assertGreaterEqual(relay.heartbeats, 10)
            self.assertIn("MAINTHREAD_DISPATCH_DELAY", text)
            self.assertIn("MAINTHREAD_DISPATCH_RECOVERED", text)
            self.assertNotIn("SESSION_ENDED", text)
        elif mode == "roster":
            self.assertNotIn("SESSION_ENDED", text)
            self.assertIn("roster-generation1-to2-after-retirement", text)
        else:
            self.assertEqual(text.count("SESSION_ENDED"), 1, text)
            self.assertIn("existing session cannot be implicitly reset", text)
        return text

    def test_engine_pause12s_keeps_live_host_and_defers_native_handlers(self):
        self.check_mode("stall")

    def test_real_host_silence_ends_without_engine_tick(self):
        self.check_mode("silent")

    def test_stale_replayed_heartbeats_do_not_keep_host_alive(self):
        self.check_mode("replay")

    def test_relay_tcp_eof_ends_promptly(self):
        self.check_mode("eof")

    def test_native_queue_limit_has_precise_cause(self):
        self.check_mode("overflow")

    def test_roster_generation_waits_for_world_retirement(self):
        self.check_mode("roster")


if __name__ == "__main__":
    unittest.main()
