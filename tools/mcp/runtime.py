"""Bound local runtime diagnostics and serialized game-thread mailbox."""
from __future__ import annotations

import ctypes
from ctypes import wintypes
import json
import hashlib
import logging
import os
import io
from pathlib import Path
import threading
import time
import uuid

LOG = logging.getLogger("mt2_mcp")
MAX_JSON = 262144
INSTALLED_PROOF_ROOT = Path(__file__).resolve().parents[2] / "_MPTest" / "dropin-proof"
INSTALLED_EXE_SHA256 = "bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6"
JSON_SHARING_WAIT_SECONDS = 0.25
JSON_SHARING_POLL_SECONDS = 0.01
STATUS_PUBLICATION_WAIT_SECONDS = 0.25
STATUS_PUBLICATION_POLL_SECONDS = 0.01
OPS = {"status", "inspect_engine", "capture_snapshot", "capture_local_snapshot", "capture_frame", "set_time_scale", "load_snapshot", "execute_action", "create_test_world", "exit_game", "toggle_overlay", "dismiss_crash_report", "probe_building_placement", "place_test_building", "purchase_test_region", "focus_test_camera", "read_subscriber_needs"}
OPS.add("append_subscriber_record_probe")
MUTATIONS = {"set_time_scale", "capture_local_snapshot", "load_snapshot", "execute_action", "create_test_world", "exit_game", "toggle_overlay", "dismiss_crash_report", "place_test_building", "purchase_test_region", "focus_test_camera"}
MUTATIONS.add("append_subscriber_record_probe")
OPS.update({"arm_subscriber_needs", "apply_subscriber_needs", "disarm_subscriber_needs"})
OPS.update({"read_network_state", "read_subscriber_candidates", "read_time_ui", "read_building_catalog"})
OPS.update({"read_time_acceptance", "native_time_key", "read_time_input"})
OPS.update({"native_time_button", "read_time_mouse"})
MUTATIONS.add("native_time_button")
OPS.update({"read_native_free", "probe_native_holder"})
OPS.update({"read_remove_bindings", "probe_remove_snapshot_building"})
OPS.add("retire_snapshot_remove")
OPS.update({"start_typed_actions", "request_snapshot_remove"})
OPS.update({"enable_ui_remove", "probe_remove_ui"})
OPS.update({"enable_ui_build", "request_building_place", "native_build_button", "read_build_ui", "native_move_ui", "read_move_ui"})
MUTATIONS.add("native_move_ui")
OPS.update({"enable_live_host_state","read_live_host_state"})
MUTATIONS.add("enable_live_host_state")
OPS.update({"native_cancel_tool", "read_cancel_tool"})
MUTATIONS.add("native_cancel_tool")
OPS.update({"enable_ui_move", "request_building_move"})
MUTATIONS.update({"enable_ui_move", "request_building_move"})
OPS.update({"enable_ui_path", "read_path", "native_path_segment", "native_path_delete"})
OPS.update({"enable_ui_network", "read_network_placement", "native_network_node", "native_network_pipe"})
MUTATIONS.update({"enable_ui_network", "native_network_node", "native_network_pipe"})
MUTATIONS.update({"enable_ui_path", "native_path_segment", "native_path_delete"})
OPS.update({"enable_ui_terrain", "read_terrain", "native_terrain_stroke", "native_terrain_height"})
MUTATIONS.update({"enable_ui_terrain", "native_terrain_stroke", "native_terrain_height"})
OPS.update({"enable_ui_region", "read_region_preset", "native_region_preset_ui", "native_region_styles_ui"})
OPS.add("read_prices")
OPS.add("read_npc_region")
OPS.add("native_npc_tool_ui")
MUTATIONS.add("native_npc_tool_ui")
OPS.add("native_npc_place_probe")
MUTATIONS.add("native_npc_place_probe")
OPS.add("native_npc_place_ui")
MUTATIONS.add("native_npc_place_ui")
OPS.add("native_npc_remove_ui")
MUTATIONS.add("native_npc_remove_ui")
OPS.add("native_npc_move_ui")
MUTATIONS.add("native_npc_move_ui")
OPS.add("native_price_ui")
MUTATIONS.add("native_price_ui")
MUTATIONS.update({"enable_ui_region", "native_region_preset_ui", "native_region_styles_ui"})
OPS.add("native_region_property_ui")
MUTATIONS.add("native_region_property_ui")
OPS.update({"native_building_history_ui", "read_building_history_ui"})
MUTATIONS.add("native_building_history_ui")
MUTATIONS.add("native_build_button")
MUTATIONS.update({"enable_ui_build", "request_building_place"})
MUTATIONS.update({"enable_ui_remove", "probe_remove_ui"})
MUTATIONS.update({"start_typed_actions", "request_snapshot_remove"})
MUTATIONS.add("retire_snapshot_remove")
MUTATIONS.add("probe_remove_snapshot_building")
MUTATIONS.add("probe_native_holder")
MUTATIONS.add("native_time_key")
MUTATIONS.update({"arm_subscriber_needs", "apply_subscriber_needs", "disarm_subscriber_needs"})


OPS.update({"read_native_lobby", "native_lobby_button"})
OPS.update({"read_chat", "send_chat"})
MUTATIONS.add("send_chat")
OPS.update({"read_chat_ui", "native_chat_ui_key", "native_chat_ui_text"})
MUTATIONS.update({"native_chat_ui_key", "native_chat_ui_text"})
OPS.update({"read_native_loans", "native_take_loan_fixture", "native_loan_payment"})
OPS.update({"read_native_release", "native_initial_release_fixture"})
MUTATIONS.add("native_initial_release_fixture")
OPS.update({"enable_node_observer", "read_node_observer"})
MUTATIONS.add("enable_node_observer")
OPS.add("capture_subscriber_record")
MUTATIONS.add("capture_subscriber_record")
MUTATIONS.update({"native_take_loan_fixture", "native_loan_payment"})
MUTATIONS.add("native_lobby_button")

class RuntimeErrorDetail(Exception):
    def __init__(self, code: str, message: str, native_response: dict | None = None):
        self.code = code
        self.native_response = native_response
        super().__init__(message)


def shared_open(path: Path) -> io.BufferedReader:
    """Allow the game to atomically replace diagnostic files while reading."""
    if os.name != "nt":
        return path.open("rb")
    import msvcrt
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel.CreateFileW.argtypes = [wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD, ctypes.c_void_p,
                                  wintypes.DWORD, wintypes.DWORD, wintypes.HANDLE]
    kernel.CreateFileW.restype = wintypes.HANDLE
    kernel.CloseHandle.argtypes = [wintypes.HANDLE]
    handle = kernel.CreateFileW(str(path), 0x80000000, 1 | 2 | 4, None, 3, 0x80, None)
    if handle == ctypes.c_void_p(-1).value:
        raise ctypes.WinError(ctypes.get_last_error())
    try:
        descriptor = msvcrt.open_osfhandle(handle, os.O_RDONLY | os.O_BINARY)
    except OSError:
        kernel.CloseHandle(handle)
        raise
    try:
        return os.fdopen(descriptor, "rb")
    except Exception:
        os.close(descriptor)
        raise


def read_json(path: Path) -> dict:
    started = time.monotonic()
    sharing_failures = 0
    previous_error = None
    first_cause = None
    try:
        while True:
            try:
                with shared_open(path) as stream:
                    raw = stream.read(MAX_JSON + 1)
                break
            except OSError as exc:
                error = getattr(exc, "winerror", None)
                if error not in {32, 33}:
                    if sharing_failures:
                        LOG.error("JSON read failed after sharing wait target=%s failures=%s cause=%s original_cause=%s",
                                  path, sharing_failures, exc, first_cause)
                    raise
                sharing_failures += 1
                if first_cause is None:
                    first_cause = str(exc)
                elapsed = time.monotonic() - started
                remaining = JSON_SHARING_WAIT_SECONDS - elapsed
                if remaining <= 0:
                    LOG.error("JSON sharing wait exhausted target=%s failures=%s elapsed_ms=%.1f required_limit_ms=%.1f cause=%s original_cause=%s",
                              path, sharing_failures, elapsed * 1000, JSON_SHARING_WAIT_SECONDS * 1000, exc, first_cause)
                    raise
                if error != previous_error:
                    LOG.info("JSON read sharing wait target=%s winerror=%s failures=%s limit_ms=%.1f cause=%s",
                             path, error, sharing_failures, JSON_SHARING_WAIT_SECONDS * 1000, exc)
                    previous_error = error
                time.sleep(min(JSON_SHARING_POLL_SECONDS, remaining))
        if sharing_failures:
            LOG.info("JSON read sharing recovered target=%s failures=%s elapsed_ms=%.1f original_cause=%s",
                     path, sharing_failures, (time.monotonic() - started) * 1000, first_cause)
        if len(raw) > MAX_JSON:
            raise RuntimeErrorDetail("oversize_json", f"{path}: exceeds {MAX_JSON} bytes")
        value = json.loads(raw)
        if not isinstance(value, dict):
            raise ValueError("expected JSON object")
        return value
    except (OSError, ValueError) as exc:
        raise RuntimeErrorDetail("read_failed", f"Read {path}: {exc}") from exc


def atomic_json(path: Path, value: dict, max_bytes: int = MAX_JSON) -> None:
    payload = json.dumps(value, ensure_ascii=False, allow_nan=False).encode("utf-8")
    if len(payload) > max_bytes:
        raise RuntimeErrorDetail("oversize_json", f"Write {path}: exceeds {max_bytes} bytes")
    temporary = path.with_name(f".{path.name}.{uuid.uuid4()}.tmp")
    try:
        with temporary.open("xb") as stream:
            stream.write(payload)
            stream.flush()
            os.fsync(stream.fileno())
        if os.name == "nt" and path.exists():
            kernel = ctypes.WinDLL("kernel32", use_last_error=True)
            kernel.ReplaceFileW.argtypes = [wintypes.LPCWSTR, wintypes.LPCWSTR, wintypes.LPCWSTR, wintypes.DWORD,
                                           ctypes.c_void_p, ctypes.c_void_p]
            if not kernel.ReplaceFileW(str(path), str(temporary), None, 0, None, None):
                raise ctypes.WinError(ctypes.get_last_error())
        else:
            os.replace(temporary, path)
    except OSError as exc:
        raise RuntimeErrorDetail("write_failed", f"Write {path}: {exc}") from exc
    finally:
        if temporary.exists():
            try:
                temporary.unlink()
            except OSError as exc:
                LOG.warning("Temporary cleanup failed target=%s cause=%s", temporary, exc)


def windows_process(pid: int) -> dict | None:
    """Return exact process identity and current-account ownership or known exit."""
    if os.name != "nt":
        raise RuntimeErrorDetail("unsupported_platform", "Live process binding requires Windows")
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    advapi = ctypes.WinDLL("advapi32", use_last_error=True)
    kernel.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
    kernel.OpenProcess.restype = wintypes.HANDLE
    kernel.CloseHandle.argtypes = [wintypes.HANDLE]
    kernel.GetCurrentProcess.restype = wintypes.HANDLE
    kernel.GetProcessTimes.argtypes = [wintypes.HANDLE] + [ctypes.POINTER(wintypes.FILETIME)] * 4
    kernel.QueryFullProcessImageNameW.argtypes = [wintypes.HANDLE, wintypes.DWORD, wintypes.LPWSTR, ctypes.POINTER(wintypes.DWORD)]
    kernel.GetExitCodeProcess.argtypes = [wintypes.HANDLE, ctypes.POINTER(wintypes.DWORD)]
    advapi.OpenProcessToken.argtypes = [wintypes.HANDLE, wintypes.DWORD, ctypes.POINTER(wintypes.HANDLE)]
    advapi.GetTokenInformation.argtypes = [wintypes.HANDLE, ctypes.c_int, ctypes.c_void_p, wintypes.DWORD, ctypes.POINTER(wintypes.DWORD)]
    advapi.EqualSid.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
    handle = kernel.OpenProcess(0x1000, False, pid)
    if not handle:
        error = ctypes.get_last_error()
        if error == 87:
            return None
        raise RuntimeErrorDetail("process_query_failed", f"OpenProcess pid={pid}: Windows error {error}")
    tokens = []
    try:
        exit_code = wintypes.DWORD()
        if not kernel.GetExitCodeProcess(handle, ctypes.byref(exit_code)):
            raise ctypes.WinError(ctypes.get_last_error())
        if exit_code.value != 259:
            return None
        creation, exit_time, system, user = (wintypes.FILETIME() for _ in range(4))
        if not kernel.GetProcessTimes(handle, *(ctypes.byref(v) for v in (creation, exit_time, system, user))):
            raise ctypes.WinError(ctypes.get_last_error())
        size = wintypes.DWORD(32768)
        executable = ctypes.create_unicode_buffer(size.value)
        if not kernel.QueryFullProcessImageNameW(handle, 0, executable, ctypes.byref(size)):
            raise ctypes.WinError(ctypes.get_last_error())
        users = []
        for process in (handle, kernel.GetCurrentProcess()):
            token = wintypes.HANDLE()
            if not advapi.OpenProcessToken(process, 8, ctypes.byref(token)):
                raise ctypes.WinError(ctypes.get_last_error())
            tokens.append(token)
            needed = wintypes.DWORD()
            advapi.GetTokenInformation(token, 1, None, 0, ctypes.byref(needed))
            if ctypes.get_last_error() != 122 or needed.value == 0:
                raise ctypes.WinError(ctypes.get_last_error())
            buffer = ctypes.create_string_buffer(needed.value)
            if not advapi.GetTokenInformation(token, 1, buffer, needed, ctypes.byref(needed)):
                raise ctypes.WinError(ctypes.get_last_error())
            users.append(buffer)
        sid_a = ctypes.cast(users[0], ctypes.POINTER(ctypes.c_void_p)).contents.value
        sid_b = ctypes.cast(users[1], ctypes.POINTER(ctypes.c_void_p)).contents.value
        return {"pid": pid, "process_start_time": str((creation.dwHighDateTime << 32) | creation.dwLowDateTime),
                "executable_path": executable.value, "owned_by_current_user": bool(advapi.EqualSid(sid_a, sid_b))}
    except OSError as exc:
        raise RuntimeErrorDetail("process_query_failed", f"Process identity pid={pid}: {exc}") from exc
    finally:
        for token in tokens:
            kernel.CloseHandle(token)
        kernel.CloseHandle(handle)


def windows_modules(pid: int) -> list[str]:
    """Read module paths for only the exact owned PID selected by Runtime."""
    if os.name != "nt":
        raise RuntimeErrorDetail("unsupported_platform", "Installed module proof requires Windows")
    class Module(ctypes.Structure):
        _fields_ = [(name, wintypes.DWORD) for name in ("dwSize", "th32ModuleID", "th32ProcessID", "GlblcntUsage", "ProccntUsage")] + [
            ("modBaseAddr", ctypes.c_void_p), ("modBaseSize", wintypes.DWORD), ("hModule", wintypes.HMODULE),
            ("szModule", wintypes.WCHAR * 256), ("szExePath", wintypes.WCHAR * 260)]
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel.CreateToolhelp32Snapshot.argtypes = [wintypes.DWORD, wintypes.DWORD]
    kernel.CreateToolhelp32Snapshot.restype = wintypes.HANDLE
    for name in ("Module32FirstW", "Module32NextW"):
        getattr(kernel, name).argtypes = [wintypes.HANDLE, ctypes.POINTER(Module)]
        getattr(kernel, name).restype = wintypes.BOOL
    kernel.CloseHandle.argtypes = [wintypes.HANDLE]
    kernel.CloseHandle.restype = wintypes.BOOL
    snapshot = ctypes.c_void_p(-1).value
    for attempt in range(3):
        snapshot = kernel.CreateToolhelp32Snapshot(0x08 | 0x10, pid)
        if snapshot != ctypes.c_void_p(-1).value:
            break
        error = ctypes.get_last_error()
        if error != 24 or attempt == 2:
            raise RuntimeErrorDetail("module_query_failed", f"Module snapshot pid={pid} attempt={attempt + 1} Windows error={error}")
        LOG.info("Module snapshot changed pid=%s attempt=%s cause=ERROR_BAD_LENGTH; bounded retry", pid, attempt + 1)
    try:
        item = Module(); item.dwSize = ctypes.sizeof(Module)
        if not kernel.Module32FirstW(snapshot, ctypes.byref(item)):
            raise RuntimeErrorDetail("module_query_failed", f"First module pid={pid} Windows error={ctypes.get_last_error()}")
        paths = []
        while True:
            if len(paths) >= 4096 or not item.szExePath or len(item.szExePath) >= 259:
                raise RuntimeErrorDetail("module_query_failed", "Module list/path bound exceeded")
            paths.append(str(Path(item.szExePath).resolve()))
            if not kernel.Module32NextW(snapshot, ctypes.byref(item)):
                error = ctypes.get_last_error()
                if error != 18:
                    raise RuntimeErrorDetail("module_query_failed", f"Next module pid={pid} Windows error={error}")
                return paths
    finally:
        if not kernel.CloseHandle(snapshot):
            LOG.error("Module snapshot close failed pid=%s Windows error=%s", pid, ctypes.get_last_error())


class Runtime:
    def __init__(self, instances_root: Path, inspector=windows_process, timeout: float = 5.0, module_inspector=windows_modules):
        self.instances_root = instances_root.resolve()
        if "_mptest" not in {part.lower() for part in self.instances_root.parts}:
            raise RuntimeErrorDetail("invalid_root", "Configured instances root must be inside _MPTest")
        if not 0.05 <= timeout <= 65:
            raise RuntimeErrorDetail("invalid_timeout", "Mailbox timeout must be 0.05 through 65 seconds")
        self.inspector = inspector
        self.module_inspector = module_inspector
        self.timeout = timeout
        self.bound: dict | None = None
        self.manifest_path: Path | None = None
        self.mutex = threading.RLock()
        self._response_absence_request = None
        self._response_absence_count = 0

    def _within(self, path: Path, root: Path) -> Path:
        actual = path.resolve()
        # Windows can return its extended drive spelling during atomic file
        # publication. Compare equivalent resolved paths without weakening
        # junction resolution or the directory boundary.
        def drive_spelling(resolved):
            value = str(resolved)
            if value.startswith('\\\\?\\') and len(value) >= 7 and value[4].isalpha() and value[5:7] == ':\\':
                return Path(value[4:])
            return resolved
        actual = drive_spelling(actual)
        boundary = drive_spelling(root.resolve())
        if not actual.is_relative_to(boundary):
            raise RuntimeErrorDetail("path_escape", f"Path {actual} is outside {root}")
        return actual

    def _manifest(self, path: Path) -> dict:
        path = self._within(path, self.instances_root)
        if path.name != "instance.json":
            raise RuntimeErrorDetail("invalid_manifest", "Manifest filename must be instance.json")
        value = read_json(path)
        return self._manifest_value(value, expected_root=path.parent)

    def _installed_manifest(self, value: dict, expected_root: Path | None) -> dict:
        if value["owner"] != {"kind": "MPTest", "run_id": value["run_id"]}:
            raise RuntimeErrorDetail("ownership_failed", "Installed manifest owner must identify this MPTest run")
        proof = value.get("installed_proof")
        required = {"acceptance_root", "game_dir", "runtime_root", "user_data_dir", "diagnostic_mailbox_enabled", "local_test", "isolation_flags", "process_start_time", "modules"}
        if not isinstance(proof, dict) or set(proof) != required:
            raise RuntimeErrorDetail("invalid_installed_proof", "Installed proof receipt fields differ")
        boundary = INSTALLED_PROOF_ROOT.absolute()
        if boundary.resolve() != boundary:
            raise RuntimeErrorDetail("path_escape", "Repository proof root contains an alias")
        if proof["diagnostic_mailbox_enabled"] is not True or proof["local_test"] is not False or type(proof["isolation_flags"]) is not int or proof["isolation_flags"] != 3:
            raise RuntimeErrorDetail("ownership_failed", "Installed proof must retain private isolation and real Steam mode")
        for key in ("acceptance_root", "game_dir", "runtime_root", "user_data_dir"):
            if not isinstance(proof[key], str) or not Path(proof[key]).is_absolute():
                raise RuntimeErrorDetail("invalid_installed_proof", f"Installed {key} must be absolute")
        game = boundary / "game"
        install_id = hashlib.sha256(str(game).lower().encode("utf-16le")).hexdigest()
        runtime = boundary / "appdata" / "MT2Multiplayer" / install_id
        self._within(runtime, self.instances_root)
        paths = {"acceptance_root": boundary, "game_dir": game, "runtime_root": runtime, "user_data_dir": runtime / "data"}
        for key, path in paths.items():
            actual = Path(proof[key])
            if actual.resolve() != path or actual.absolute() != path:
                raise RuntimeErrorDetail("path_escape", f"Installed {key} differs from exact owned proof layout")
            for ancestor in (actual, *actual.parents):
                if ancestor.is_symlink() or (hasattr(ancestor, "is_junction") and ancestor.is_junction()):
                    raise RuntimeErrorDetail("path_escape", f"Installed {key} contains a reparse alias")
        if Path(value["runtime_root"]).resolve() != runtime or (expected_root is not None and expected_root != runtime):
            raise RuntimeErrorDetail("invalid_manifest", "Installed manifest must be in its exact private runtime root")
        executable = game / "MT2.exe"
        if Path(value["executable_path"]).resolve() != executable or proof["process_start_time"] != value["process_start_time"]:
            raise RuntimeErrorDetail("ownership_failed", "Installed executable/start token differs")
        modules = proof["modules"]
        filenames = {"executable": "MT2.exe", "loader": "WSOCK32.dll", "mod": "MT2_Multiplayer.dll", "steam": "steam_api64.dll"}
        if not isinstance(modules, dict) or set(modules) != set(filenames):
            raise RuntimeErrorDetail("invalid_installed_proof", "Installed four-module receipt differs")
        for key, filename in filenames.items():
            item = modules[key]
            target = game / filename
            if not isinstance(item, dict) or set(item) != {"path", "sha256", "bytes"} or not isinstance(item["path"], str) or Path(item["path"]).resolve() != target:
                raise RuntimeErrorDetail("ownership_failed", f"Installed module path differs: {key}")
            if not isinstance(item["sha256"], str) or len(item["sha256"]) != 64 or any(c not in "0123456789abcdef" for c in item["sha256"]):
                raise RuntimeErrorDetail("invalid_installed_proof", f"Installed module hash invalid: {key}")
            if type(item["bytes"]) is not int or item["bytes"] <= 0:
                raise RuntimeErrorDetail("invalid_installed_proof", f"Installed module byte count invalid: {key}")
            try:
                actual_size = target.stat().st_size
                digest = hashlib.sha256()
                with target.open("rb") as stream:
                    for block in iter(lambda: stream.read(1024 * 1024), b""):
                        digest.update(block)
            except OSError as exc:
                raise RuntimeErrorDetail("module_read_failed", f"Read installed module {key}: {exc}") from exc
            if type(item["bytes"]) is not int or item["bytes"] <= 0 or actual_size != item["bytes"] or digest.hexdigest() != item["sha256"]:
                raise RuntimeErrorDetail("module_changed", f"Installed module hash/size differs: {key}")
        if modules["executable"]["sha256"] != INSTALLED_EXE_SHA256:
            raise RuntimeErrorDetail("ownership_failed", "Installed executable exact-build hash differs")
        value["runtime_root"] = str(runtime); value["executable_path"] = str(executable)
        return value

    def _manifest_value(self, value: dict, expected_root: Path | None = None) -> dict:
        required = {"schema_version", "run_id", "pid", "process_start_time", "runtime_root", "executable_path", "owner"}
        if not required.issubset(value) or value.get("schema_version") != 1:
            raise RuntimeErrorDetail("invalid_manifest", "Manifest missing fields or wrong schema")
        try:
            uuid.UUID(value["run_id"])
        except (ValueError, TypeError, AttributeError) as exc:
            raise RuntimeErrorDetail("invalid_manifest", "Manifest run_id must be UUID") from exc
        if type(value["pid"]) is not int or value["pid"] <= 0:
            raise RuntimeErrorDetail("invalid_manifest", "Manifest pid must be positive integer")
        if not isinstance(value["process_start_time"], str) or not value["process_start_time"].isdigit():
            raise RuntimeErrorDetail("invalid_manifest", "Manifest process_start_time must be decimal FILETIME string")
        for field in ("runtime_root", "executable_path"):
            if not isinstance(value[field], str) or not Path(value[field]).is_absolute():
                raise RuntimeErrorDetail("invalid_manifest", f"Manifest {field} must be absolute path")
        if value.get("profile") == "installed_dropin_proof":
            return self._installed_manifest(value, expected_root)
        root = self._within(Path(value["runtime_root"]), self.instances_root)
        executable = self._within(Path(value["executable_path"]), root)
        if (expected_root is not None and root != expected_root) or executable.name.lower() != "mt2.exe":
            raise RuntimeErrorDetail("invalid_manifest", "Manifest must be in runtime root with an MT2.exe executable")
        if value["owner"] != {"kind": "MPTest", "run_id": value["run_id"]}:
            raise RuntimeErrorDetail("ownership_failed", "Manifest owner must identify this MPTest run")
        value["runtime_root"] = str(root)
        value["executable_path"] = str(executable)
        return value

    def recover_pending(self, previous_manifest_path: str) -> dict:
        with self.mutex:
            path = Path(previous_manifest_path).resolve()
            evidence_root = Path(__file__).resolve().parents[2] / "test" / "mcp" / "evidence"
            if path.name != "instance.json" or not (path.is_relative_to(self.instances_root) or path.is_relative_to(evidence_root.resolve())):
                raise RuntimeErrorDetail("invalid_receipt", "Previous manifest must be instance.json inside owned test roots or MCP evidence")
            previous = self._manifest_value(read_json(path))
            if self.bound and previous["runtime_root"] != self.bound["runtime_root"]:
                raise RuntimeErrorDetail("ownership_failed", "Previous manifest root differs from bound runtime")
            root = Path(previous["runtime_root"])
            diagnostics = self._within(root / "diagnostics", root)
            lock = self._within(diagnostics / "request.lock", diagnostics)
            if not lock.exists():
                return {"pending": False, "reason": "No pending lock in previous owned runtime"}
            record = read_json(lock)
            if any(record.get(key) != previous[key] for key in ("run_id", "pid", "process_start_time")):
                raise RuntimeErrorDetail("receipt_mismatch", "Pending lock does not match previous manifest PID/start/run identity")
            for key in ("runtime_root", "executable_path", "owner"):
                if key in record and record[key] != previous[key]:
                    raise RuntimeErrorDetail("receipt_mismatch", f"Pending lock {key} differs from previous manifest")
            try:
                uuid.UUID(record.get("id"))
            except (ValueError, TypeError, AttributeError) as exc:
                raise RuntimeErrorDetail("invalid_lock", "Pending lock lacks valid request UUID") from exc
            try:
                alive = self._identity(previous)
            except RuntimeErrorDetail as exc:
                if exc.code != "pid_reused":
                    raise
                LOG.warning("Recovery confirms previous process exit id=%s cause=%s", record["id"], exc)
                alive = False
            if alive:
                raise RuntimeErrorDetail("prior_process_alive", f"Previous process pid={previous['pid']} is still alive; lock retained")
            self._release(lock, record["id"], expected_run_id=previous["run_id"])
            LOG.warning("Recovered pending lock after exact previous manifest exit id=%s pid=%s run_id=%s", record["id"], previous["pid"], previous["run_id"])
            return {"pending": False, "abandoned_after_process_exit": True, "request_id": record["id"],
                    "previous_run_id": previous["run_id"], "previous_pid": previous["pid"], "process_exit_verified": True}

    def _identity(self, manifest: dict) -> bool:
        observed = self.inspector(manifest["pid"])
        if observed is None:
            return False
        if observed["process_start_time"] != manifest["process_start_time"]:
            raise RuntimeErrorDetail("pid_reused", f"Process pid={manifest['pid']} start token changed: observed={observed['process_start_time']} required={manifest['process_start_time']}")
        if Path(observed["executable_path"]).resolve() != Path(manifest["executable_path"]):
            raise RuntimeErrorDetail("ownership_failed", f"Executable identity differs for pid={manifest['pid']}")
        if observed.get("owned_by_current_user") is not True:
            raise RuntimeErrorDetail("ownership_failed", f"Process pid={manifest['pid']} belongs to another account")
        if manifest.get("profile") == "installed_dropin_proof":
            loaded = {Path(path).resolve() for path in self.module_inspector(manifest["pid"])}
            expected = {Path(item["path"]).resolve() for item in manifest["installed_proof"]["modules"].values()}
            if not expected.issubset(loaded):
                raise RuntimeErrorDetail("module_identity_failed", "Installed process lacks an exact receipt module path")
        return True

    def list_instances(self) -> dict:
        entries = []
        if self.instances_root.exists():
            paths = sorted(self.instances_root.glob("instance.json")) + sorted(self.instances_root.glob("*/instance.json"))
            for path in paths[:32]:
                try:
                    manifest = self._manifest(path)
                    entries.append({"manifest_path": str(path), "run_id": manifest["run_id"], "pid": manifest["pid"],
                                    "alive": self._identity(manifest)})
                except RuntimeErrorDetail as exc:
                    LOG.warning("Instance inspection failed target=%s code=%s cause=%s", path, exc.code, exc)
                    entries.append({"manifest_path": str(path), "error": {"code": exc.code, "message": str(exc)}})
        return {"instances": entries, "limit": 32}

    def bind(self, manifest_path: str) -> dict:
        with self.mutex:
            candidate = self._manifest(Path(manifest_path))
            if self.bound and self.bound != candidate:
                if self._diagnostics().joinpath("request.lock").exists():
                    raise RuntimeErrorDetail("pending_request", "Resolve existing runtime request before rebinding")
            if not self._identity(candidate):
                raise RuntimeErrorDetail("process_exited", f"Manifest process pid={candidate['pid']} is absent or reused")
            self.bound, self.manifest_path = candidate, Path(manifest_path).resolve()
            LOG.info("Bound runtime run_id=%s pid=%s root=%s", candidate["run_id"], candidate["pid"], candidate["runtime_root"])
            return {"bound": dict(candidate), "process_identity_verified": True}

    def _diagnostics(self) -> Path:
        if not self.bound:
            raise RuntimeErrorDetail("not_bound", "Bind one owned runtime before this operation")
        return self._within(Path(self.bound["runtime_root"]) / "diagnostics", Path(self.bound["runtime_root"]))

    def _current(self) -> dict:
        if not self.bound or not self.manifest_path:
            raise RuntimeErrorDetail("not_bound", "Bind one owned runtime before this operation")
        if self._manifest(self.manifest_path) != self.bound:
            raise RuntimeErrorDetail("manifest_changed", "Bound manifest identity changed; explicit rebind required")
        if not self._identity(self.bound):
            raise RuntimeErrorDetail("process_exited", f"Bound process pid={self.bound['pid']} is absent or reused")
        return self.bound

    def status(self) -> dict:
        with self.mutex:
            started = time.monotonic()
            failures = 0
            first_cause = None
            last_failure = None
            target = None
            while True:
                elapsed = time.monotonic() - started
                if failures and elapsed >= STATUS_PUBLICATION_WAIT_SECONDS:
                    LOG.error("Status publication wait exhausted target=%s failures=%s elapsed_ms=%.1f required_limit_ms=%.1f cause=%s original_cause=%s",
                              target, failures, elapsed * 1000, STATUS_PUBLICATION_WAIT_SECONDS * 1000, last_failure, first_cause)
                    raise last_failure
                reading_status = False
                try:
                    bound = self._current()
                    target = self._within(self._diagnostics() / "status.json", self._diagnostics())
                    reading_status = True
                    status = read_json(target)
                    reading_status = False
                    if status.get("schema_version") != 1 or status.get("run_id") != bound["run_id"] or status.get("pid") != bound["pid"]:
                        raise RuntimeErrorDetail("stale_status", "Runtime status identity does not match bound process")
                    if bound.get("profile") == "installed_dropin_proof" and status.get("installed_proof") != bound["installed_proof"]:
                        raise RuntimeErrorDetail("stale_status", "Installed status proof differs from exact bound module/profile receipt")
                    if type(status.get("game_thread_ready")) is not bool or not isinstance(status.get("capabilities"), dict):
                        raise RuntimeErrorDetail("invalid_status", "Runtime status lacks readiness or capabilities")
                    self._current()
                    if failures:
                        LOG.info("Status publication recovered target=%s failures=%s elapsed_ms=%.1f original_cause=%s",
                                 target, failures, (time.monotonic() - started) * 1000, first_cause)
                    return status
                except RuntimeErrorDetail as exc:
                    native = exc.__cause__
                    if not (reading_status and exc.code == "read_failed" and isinstance(native, OSError) and getattr(native, "winerror", None) == 2):
                        if failures:
                            LOG.error("Status publication wait aborted target=%s failures=%s elapsed_ms=%.1f cause=%s original_cause=%s",
                                      target, failures, (time.monotonic() - started) * 1000, exc, first_cause)
                        raise
                    failures += 1
                    if first_cause is None:
                        first_cause = str(native)
                    last_failure = exc
                    elapsed = time.monotonic() - started
                    remaining = STATUS_PUBLICATION_WAIT_SECONDS - elapsed
                    if remaining <= 0:
                        LOG.error("Status publication wait exhausted target=%s failures=%s elapsed_ms=%.1f required_limit_ms=%.1f cause=%s original_cause=%s",
                                  target, failures, elapsed * 1000, STATUS_PUBLICATION_WAIT_SECONDS * 1000, exc, first_cause)
                        raise
                    if failures == 1:
                        LOG.info("Status publication wait target=%s failures=%s limit_ms=%.1f cause=%s; bound identity rechecked before each read",
                                 target, failures, STATUS_PUBLICATION_WAIT_SECONDS * 1000, native)
                    time.sleep(min(STATUS_PUBLICATION_POLL_SECONDS, remaining))

    def available(self) -> set[str]:
        if not self.bound:
            return set()
        status = self.status()
        if status["game_thread_ready"] is not True:
            return set()
        return {op for op in OPS if status["capabilities"].get(op) is True}

    def read_logs(self, filename: str = "mt2mp.log", max_bytes: int = 16384) -> dict:
        with self.mutex:
            self._current()
            if filename not in {"runtime.log", "mt2mp.log", "diagnostics.log"}:
                raise RuntimeErrorDetail("invalid_log", "Log filename is not in the diagnostics allowlist")
            if type(max_bytes) is not int or not 1 <= max_bytes <= 65536:
                raise RuntimeErrorDetail("invalid_bound", "max_bytes must be integer 1 through 65536")
            root = Path(self.bound["runtime_root"])
            path = self._within(root / "game" / filename, root) if filename == "mt2mp.log" else self._within(self._diagnostics() / filename, self._diagnostics())
            try:
                with shared_open(path) as stream:
                    stream.seek(0, 2)
                    length = stream.tell()
                    stream.seek(max(0, length - max_bytes))
                    raw = stream.read(max_bytes)
                return {"path": str(path), "text": raw.decode("utf-8", errors="replace"), "truncated": length > max_bytes}
            except OSError as exc:
                raise RuntimeErrorDetail("log_read_failed", f"Read log {path}: {exc}") from exc

    def _matching_response(self, request_id: str) -> dict | None:
        if self._response_absence_request != request_id:
            self._response_absence_request = request_id
            self._response_absence_count = 0
        path = self._within(self._diagnostics() / "response.json", self._diagnostics())
        if not path.exists():
            return None
        try:
            response = read_json(path)
        except RuntimeErrorDetail as exc:
            cause = exc.__cause__
            if exc.code != 'read_failed' or not isinstance(cause, OSError) or getattr(cause, 'winerror', None) != 2:
                raise
            self._response_absence_count += 1
            count = self._response_absence_count
            if count == 1 or count & (count - 1) == 0:
                LOG.info('Response publication wait id=%s target=%s count=%s cause=%s; lock retained, no replay', request_id, path, count, cause)
            return None
        if self._response_absence_count:
            LOG.info('Response publication recovered id=%s target=%s count=%s; matching acknowledgement still required', request_id, path, self._response_absence_count)
            self._response_absence_count = 0
        if response.get("id") != request_id or response.get("run_id") != self.bound["run_id"]:
            return None
        if response.get("schema_version") != 1 or type(response.get("ok")) is not bool:
            raise RuntimeErrorDetail("invalid_response", f"Response id={request_id} has invalid schema")
        if response["ok"] and not isinstance(response.get("result"), dict):
            raise RuntimeErrorDetail("invalid_response", f"Response id={request_id} lacks object result")
        if not response["ok"]:
            error = response.get("error")
            if not isinstance(error, dict) or not all(isinstance(error.get(key), str) and error[key] for key in ("code", "message")):
                raise RuntimeErrorDetail("invalid_response", f"Response id={request_id} lacks error code or message")
        return response

    def _release(self, lock: Path, expected_id: str, expected_run_id: str | None = None) -> None:
        actual = read_json(lock)
        if actual.get("id") != expected_id or actual.get("run_id") != (expected_run_id or self.bound["run_id"]):
            raise RuntimeErrorDetail("lock_changed", "Mailbox lock changed; release refused")
        try:
            lock.unlink()
        except OSError as exc:
            raise RuntimeErrorDetail("lock_release_failed", f"Release {lock}: {exc}") from exc

    def observe_pending(self) -> dict:
        with self.mutex:
            diagnostics = self._diagnostics()
            lock = self._within(diagnostics / "request.lock", diagnostics)
            if not lock.exists():
                return {"pending": False}
            record = read_json(lock)
            if any(record.get(key) != self.bound[key] for key in ("run_id", "pid", "process_start_time")):
                return self._observe_previous_run(lock, record)
            try:
                uuid.UUID(record.get("id"))
            except (ValueError, TypeError, AttributeError) as exc:
                raise RuntimeErrorDetail("invalid_lock", "Mailbox lock lacks valid request UUID; retained") from exc
            response = self._matching_response(record.get("id"))
            if response is not None:
                self._release(lock, record["id"])
                LOG.info("Pending request resolved id=%s ok=%s", record["id"], response["ok"])
                return {"pending": False, "response": response}
            try:
                alive = self._identity(self.bound)
            except RuntimeErrorDetail as exc:
                if exc.code != "pid_reused":
                    raise
                LOG.warning("Original pending process exited id=%s cause=%s", record["id"], exc)
                alive = False
            if not alive:
                self._release(lock, record["id"])
                LOG.warning("Pending request abandoned after process exit id=%s", record["id"])
                return {"pending": False, "abandoned_after_process_exit": True, "request_id": record["id"]}
            return {"pending": True, "request_id": record["id"], "reason": "Matching response not observed; lock retained"}

    def _observe_previous_run(self, lock: Path, record: dict) -> dict:
        self._current()
        required = {"schema_version", "id", "run_id", "pid", "process_start_time", "runtime_root", "executable_path", "owner"}
        if not required.issubset(record) or record.get("schema_version") != 1:
            raise RuntimeErrorDetail("foreign_lock", "Prior mailbox lock lacks exact owned runtime identity; retained")
        try:
            uuid.UUID(record["id"])
            uuid.UUID(record["run_id"])
        except (ValueError, TypeError, AttributeError) as exc:
            raise RuntimeErrorDetail("foreign_lock", "Prior mailbox lock lacks UUID identity; retained") from exc
        if type(record["pid"]) is not int or record["pid"] <= 0 or not isinstance(record["process_start_time"], str) or not record["process_start_time"].isdigit():
            raise RuntimeErrorDetail("foreign_lock", "Prior mailbox lock lacks process identity; retained")
        if any(record.get(key) != self.bound[key] for key in ("runtime_root", "executable_path")) or record["owner"] != {"kind": "MPTest", "run_id": record["run_id"]}:
            raise RuntimeErrorDetail("foreign_lock", "Prior mailbox lock is not owned by this exact test root; retained")
        try:
            alive = self._identity(record)
        except RuntimeErrorDetail as exc:
            if exc.code != "pid_reused":
                raise
            LOG.warning("Prior-run process exited id=%s cause=%s", record["id"], exc)
            alive = False
        if alive:
            raise RuntimeErrorDetail("prior_process_alive", f"Prior runtime pid={record['pid']} is still alive; lock retained")
        self._release(lock, record["id"], expected_run_id=record["run_id"])
        LOG.warning("Prior-run request abandoned after confirmed exit id=%s run_id=%s", record["id"], record["run_id"])
        return {"pending": False, "abandoned_after_process_exit": True, "request_id": record["id"], "previous_run_id": record["run_id"]}

    def request(self, op: str, args: dict) -> dict:
        with self.mutex:
            self._current()
            if op not in OPS or op not in self.available():
                raise RuntimeErrorDetail("capability_unavailable", f"Game-thread operation {op} is not verified available")
            if not isinstance(args, dict):
                raise RuntimeErrorDetail("invalid_args", "Operation arguments must be object")
            diagnostics = self._diagnostics()
            diagnostics.mkdir(exist_ok=True)
            lock = self._within(diagnostics / "request.lock", diagnostics)
            request_id = str(uuid.uuid4())
            record = {"schema_version": 1, "id": request_id, "run_id": self.bound["run_id"], "pid": self.bound["pid"],
                      "process_start_time": self.bound["process_start_time"], "op": op,
                      "runtime_root": self.bound["runtime_root"], "executable_path": self.bound["executable_path"],
                      "owner": self.bound["owner"]}
            try:
                with lock.open("x", encoding="utf-8") as stream:
                    json.dump(record, stream)
                    stream.flush()
                    os.fsync(stream.fileno())
            except FileExistsError as exc:
                raise RuntimeErrorDetail("mailbox_busy", "Mailbox has unresolved request; call observe_pending") from exc
            except OSError as exc:
                raise RuntimeErrorDetail("lock_create_failed", f"Create {lock}: {exc}") from exc
            published = False
            try:
                self._current()
                atomic_json(self._within(diagnostics / "request.json", diagnostics),
                            {"schema_version": 1, "id": request_id, "run_id": self.bound["run_id"], "op": op, "args": args})
                published = True
                LOG.info("Game-thread request attempted id=%s op=%s run_id=%s", request_id, op, self.bound["run_id"])
                deadline = time.monotonic() + self.timeout
                while True:
                    response = self._matching_response(request_id)
                    if response is not None:
                        self._release(lock, request_id)
                        if not response["ok"]:
                            error = response["error"]
                            raise RuntimeErrorDetail(error["code"], error["message"], native_response=response)
                        LOG.info("Game-thread request confirmed id=%s op=%s", request_id, op)
                        return response
                    self._current()
                    remaining = deadline - time.monotonic()
                    if remaining <= 0:
                        raise RuntimeErrorDetail("observation_timeout", f"Request id={request_id} op={op}: no matching response within {self.timeout}s; lock retained; do not replay")
                    time.sleep(min(0.025, remaining))
            except Exception as exc:
                if not published:
                    released = self._abort_unpublished(lock, request_id, exc)
                    code = exc.code if isinstance(exc, RuntimeErrorDetail) else "publication_failed"
                    state = "released after verifying no matching request or response" if released else "retained because publication is uncertain"
                    raise RuntimeErrorDetail(code, f"{exc}; request id={request_id} publication failed; lock {state}") from exc
                raise

    def _abort_unpublished(self, lock: Path, request_id: str, failure: Exception) -> bool:
        try:
            record = read_json(lock)
            if record.get("id") != request_id or record.get("run_id") != self.bound["run_id"]:
                LOG.error("Publication cleanup refused id=%s cause=lock identity changed; lock retained; original=%s", request_id, failure)
                return False
            record["publication_state"] = "failed"
            record["publication_error"] = {"type": type(failure).__name__, "message": str(failure)}
            try:
                atomic_json(lock, record)
            except Exception as marker_failure:
                LOG.warning("Publication marker write failed id=%s cause=%s; original=%s", request_id, marker_failure, failure)
            for filename in ("request.json", "response.json"):
                path = self._within(self._diagnostics() / filename, self._diagnostics())
                if path.exists():
                    observed = read_json(path)
                    if observed.get("id") == request_id and observed.get("run_id") == self.bound["run_id"]:
                        LOG.error("Publication uncertain id=%s cause=%s; matching %s exists; lock retained", request_id, failure, filename)
                        return False
            self._release(lock, request_id)
            LOG.error("Request publication failed id=%s cause=%s; no matching request or response exists; lock released", request_id, failure)
            return True
        except Exception as observation_failure:
            LOG.error("Publication verification failed id=%s cause=%s; lock retained; original=%s", request_id, observation_failure, failure)
            return False
