"""Execute one saved, bounded MCP job against one explicitly owned instance."""
from __future__ import annotations

import argparse
import asyncio
from datetime import datetime, timedelta, timezone
import hashlib
import json
import math
from pathlib import Path
import re
import sys

from mcp import ClientSession, StdioServerParameters
from mcp.client.stdio import stdio_client

from runtime import OPS, RuntimeErrorDetail, atomic_json, read_json
from error_details import error_details, original_causes, full_trace

ALLOWED_TOOLS = {f"game_{operation}" for operation in OPS} | {"runtime_status", "read_logs", "observe_pending", "recover_pending"}


def load_spec(path: Path) -> dict:
    spec = read_json(path)
    if set(spec) - {"timeout_seconds", "calls"}:
        raise RuntimeErrorDetail("invalid_job", "Job fields are calls and optional timeout_seconds")
    timeout = spec.get("timeout_seconds", 45)
    if not isinstance(timeout, (int, float)) or isinstance(timeout, bool) or not math.isfinite(timeout) or not 0.05 <= timeout <= 65:
        raise RuntimeErrorDetail("invalid_job", "timeout_seconds must be finite number 0.05 through 65")
    calls = spec.get("calls")
    if not isinstance(calls, list) or not 1 <= len(calls) <= 32:
        raise RuntimeErrorDetail("invalid_job", "Job must contain 1 through 32 calls")
    for index, call in enumerate(calls):
        if not isinstance(call, dict) or set(call) - {"tool", "arguments"}:
            raise RuntimeErrorDetail("invalid_job", f"Call {index} must contain tool and optional arguments")
        if call.get("tool") not in ALLOWED_TOOLS:
            raise RuntimeErrorDetail("invalid_job", f"Call {index} names unavailable tool {call.get('tool')}")
        if not isinstance(call.get("arguments", {}), dict):
            raise RuntimeErrorDetail("invalid_job", f"Call {index} arguments must be object")
    return {"timeout_seconds": timeout, "calls": calls}


async def execute_job(manifest: Path, spec: dict, evidence: dict, persist, log_stream, session_factory=None) -> None:
    async def calls(client):
        evidence["step"] = "initialize"
        initialized = await client.initialize()
        evidence["protocol_version"] = initialized.protocolVersion
        persist()
        recovery_only = all(call["tool"] == "recover_pending" for call in spec["calls"])
        evidence["bind_skipped_for_recovery_only"] = recovery_only
        planned = spec["calls"] if recovery_only else [{"tool": "bind_runtime", "arguments": {"manifest_path": str(manifest)}}, *spec["calls"]]
        for index, call in enumerate(planned):
            record = {"index": index, "tool": call["tool"], "arguments": call.get("arguments", {}),
                      "state": "attempted", "started_utc": datetime.now(timezone.utc).isoformat()}
            evidence["step"] = call["tool"]
            evidence["calls"].append(record)
            persist()
            try:
                result = await client.call_tool(call["tool"], call.get("arguments", {}))
            except Exception as exc:
                record["state"] = "failed"
                record["cause"] = f"{type(exc).__name__}: {exc}"
                record["error_details"] = error_details(exc)
                persist()
                raise
            record["result"] = result.model_dump(mode="json", exclude_none=True)
            record["finished_utc"] = datetime.now(timezone.utc).isoformat()
            record["state"] = "failed" if result.isError else "confirmed"
            persist()
            if result.isError:
                raise RuntimeErrorDetail("tool_failed", f"Job call {index} {call['tool']}: {record['result']['content']}; remaining calls skipped")
        evidence["step"] = "complete"

    if session_factory is not None:
        async with session_factory() as client:
            await calls(client)
        return
    root = manifest.parent.parent
    parameters = StdioServerParameters(command=sys.executable,
                                       args=["-X", "utf8", str(Path(__file__).with_name("server.py")),
                                             "--instances-root", str(root), "--timeout", str(spec["timeout_seconds"])])
    async with stdio_client(parameters, errlog=log_stream) as (read, write):
        async with ClientSession(read, write, read_timeout_seconds=timedelta(seconds=spec["timeout_seconds"] + 15)) as client:
            await calls(client)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--spec", type=Path, required=True)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--label", default="job")
    args = parser.parse_args()
    if not re.fullmatch(r"[A-Za-z0-9_-]{1,32}", args.label):
        parser.error("label must contain 1 through 32 ASCII letters, digits, underscores or hyphens")
    workspace = Path(__file__).resolve().parents[2]
    directory = workspace / "test" / "mcp" / "evidence"
    directory.mkdir(exist_ok=True)
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")
    output = directory / f"{args.label}_{stamp}.json"
    log_path = output.with_suffix(".log")
    evidence = {"utc_time": datetime.now(timezone.utc).isoformat(), "manifest_path": str(args.manifest.resolve()),
                "spec_path": str(args.spec.resolve()), "passed": False, "calls": [], "retries": 0,
                "confirmation_scope": "matching runtime acknowledgements; inspect result fields for execution versus requested state"}
    def persist():
        atomic_json(output, evidence, max_bytes=20 * 1024 * 1024)
    try:
        spec = load_spec(args.spec)
        evidence["spec"] = spec
        evidence["spec_sha256"] = hashlib.sha256(args.spec.read_bytes()).hexdigest()
        persist()
        with log_path.open("w", encoding="utf-8") as log_stream:
            asyncio.run(execute_job(args.manifest.resolve(), spec, evidence, persist, log_stream))
        evidence["passed"] = True
    except Exception as exc:
        details = error_details(exc)
        evidence["error"] = {"operation": evidence.get("step", "startup"), "cause": f"{type(exc).__name__}: {exc}",
                             "details": details, "original_causes": original_causes(details)}
        with log_path.open("a", encoding="utf-8") as log_stream:
            log_stream.write(f"\nerror: MCP job failed operation={evidence['error']['operation']}\n{full_trace(exc)}")
        print(f"error: MCP job {evidence['error']['operation']}: {'; '.join(evidence['error']['original_causes'])}", file=sys.stderr)
    persist()
    print(json.dumps({"passed": evidence["passed"], "evidence": str(output), "log": str(log_path)}))
    raise SystemExit(0 if evidence["passed"] else 1)


if __name__ == "__main__":
    main()
