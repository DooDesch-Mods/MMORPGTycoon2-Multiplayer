"""Read-only MCP acceptance probe against one explicitly selected game instance."""
import argparse
import asyncio
from datetime import datetime, timedelta, timezone
import json
from pathlib import Path
import re
import sys

from mcp import ClientSession, StdioServerParameters
from mcp.client.stdio import stdio_client
from error_details import error_details, original_causes, full_trace


async def probe(manifest: Path, evidence: dict, log_stream) -> None:
    root = manifest.parent.parent
    parameters = StdioServerParameters(command=sys.executable,
                                       args=["-X", "utf8", str(Path(__file__).with_name("server.py")),
                                             "--instances-root", str(root), "--timeout", "5"])
    async with stdio_client(parameters, errlog=log_stream) as (read, write):
        async with ClientSession(read, write, read_timeout_seconds=timedelta(seconds=15)) as client:
            evidence["step"] = "initialize"
            initialized = await client.initialize()
            evidence["protocol_version"] = initialized.protocolVersion
            for name, args in (("bind_runtime", {"manifest_path": str(manifest)}),
                               ("runtime_status", {}), ("game_status", {}), ("game_inspect_engine", {})):
                evidence["step"] = name
                result = await client.call_tool(name, args)
                record = result.model_dump(mode="json", exclude_none=True)
                evidence["calls"].append({"tool": name, "result": record})
                if result.isError:
                    raise RuntimeError(f"{name}: {record['content']}")
            evidence["step"] = "tools/list"
            tools = await client.list_tools()
            evidence["tools"] = [tool.name for tool in tools.tools]
            evidence["step"] = "complete"


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--label", default="live")
    args = parser.parse_args()
    if not re.fullmatch(r"[A-Za-z0-9_-]{1,32}", args.label):
        parser.error("label must contain 1 through 32 ASCII letters, digits, underscores or hyphens")
    manifest = args.manifest.resolve()
    workspace = Path(__file__).resolve().parents[2]
    evidence_root = workspace / "test" / "mcp" / "evidence"
    evidence_root.mkdir(exist_ok=True)
    output = evidence_root / f"{args.label}_smoke.json"
    log_path = evidence_root / f"{args.label}_smoke.log"
    evidence = {"manifest_path": str(manifest), "utc_time": datetime.now(timezone.utc).isoformat(),
                "scope": "read-only bind, runtime status, game-thread status and engine inspection", "calls": [], "passed": False}
    try:
        with log_path.open("w", encoding="utf-8") as log_stream:
            asyncio.run(probe(manifest, evidence, log_stream))
        evidence["passed"] = True
    except Exception as exc:
        details = error_details(exc)
        evidence["error"] = {"operation": evidence.get("step", "startup"), "cause": f"{type(exc).__name__}: {exc}",
                             "details": details, "original_causes": original_causes(details)}
        with log_path.open("a", encoding="utf-8") as log_stream:
            log_stream.write(f"\nerror: Live MCP failed operation={evidence['error']['operation']}\n{full_trace(exc)}")
        print(f"error: Live MCP operation {evidence['error']['operation']} failed: {'; '.join(evidence['error']['original_causes'])}", file=sys.stderr)
    output.write_text(json.dumps(evidence, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps({"passed": evidence["passed"], "evidence": str(output), "log": str(log_path)}))
    raise SystemExit(0 if evidence["passed"] else 1)


if __name__ == "__main__":
    main()
