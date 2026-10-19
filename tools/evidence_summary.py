"""Print bounded native MCP results from a retained job receipt."""
import json
import sys
from pathlib import Path

record = json.loads(Path(sys.argv[1]).read_text(encoding="utf-8"))
print(json.dumps({"passed": record.get("passed"), "step": record.get("step"), "error": record.get("error")}, ensure_ascii=False))
for call in record.get("calls", []):
    if len(sys.argv)>2 and call["tool"]!=sys.argv[2]:
        continue
    result = call.get("result", {}).get("structuredContent")
    if isinstance(result, dict):
        for state in (result.get("result", {}), result.get("result", {}).get("engine", {})):
            if isinstance(state, dict) and "props" in state:
                props = state.pop("props")
                state["prop_count_in_receipt"] = len(props)
                state["prop_sample"] = props[:3]
    print(json.dumps({"tool": call["tool"], "state": call["state"], "result": result, "error": call.get("error")}, ensure_ascii=False))
