"""Isolated stdio/mailbox fixture with explicit synthetic process identity."""
import argparse
import logging
from pathlib import Path
import sys

import anyio

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools" / "mcp"))
from runtime import Runtime, read_json
from server import serve

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--instances-root", type=Path, required=True)
options = parser.parse_args()
manifest = read_json(options.instances_root / "host" / "instance.json")
identity = {key: manifest[key] for key in ("pid", "process_start_time", "executable_path")}
identity["owned_by_current_user"] = True
logging.basicConfig(level=logging.INFO, format="%(levelname)s %(message)s", stream=sys.stderr)
runtime = Runtime(options.instances_root, inspector=lambda pid: identity if pid == identity["pid"] else None, timeout=2)
anyio.run(serve, runtime)
