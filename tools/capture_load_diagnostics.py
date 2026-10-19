"""Retain selected native load diagnostics from one exact owned test run."""
import argparse
from datetime import datetime, timezone
import json
from pathlib import Path
import sys
import uuid

from local_acceptance import read_json

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--expected-run", required=True)
    args = parser.parse_args()
    try:
        expected = str(uuid.UUID(args.expected_run))
        source = ROOT / "_MPTest/host/diagnostics/status.json"
        status = read_json(source)
        if status.get("run_id") != expected:
            raise ValueError(f"status run differs actual={status.get('run_id')} required={expected}")
        value = {"scope": "native load observation; no completed-load claim", "source": str(source),
                 "captured_utc": datetime.now(timezone.utc).isoformat(),
                 **{key: status[key] for key in ("run_id", "pid", "native_world", "local_snapshot_load", "load_identity", "load_clock_pause")}}
        directory = ROOT / "artifacts/load_diagnostics" / expected
        directory.mkdir(parents=True, exist_ok=True)
        stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")
        output = directory / f"{stamp}.json"
        with output.open("x", encoding="utf-8") as stream:
            json.dump(value, stream, ensure_ascii=False, indent=2)
        if json.loads(output.read_text(encoding="utf-8")) != value:
            raise ValueError(f"persisted diagnostic readback differs target={output}")
        print(json.dumps({"passed": True, "evidence": str(output),
                          "elapsed_ms": value["local_snapshot_load"]["elapsed_ms"],
                          "registrations": value["load_identity"]["registrations"]}))
        return 0
    except Exception as error:
        print(f"error: retain load diagnostics run={args.expected_run} cause={type(error).__name__}: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
