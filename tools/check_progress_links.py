"""Check local progress links and media without controlling a browser."""
from html.parser import HTMLParser
from pathlib import Path
from urllib.parse import urlparse, unquote
import argparse
import re

root = Path(__file__).resolve().parents[1]
arguments = argparse.ArgumentParser(description=__doc__)
arguments.add_argument("--documents", nargs="*", type=Path, default=[])
options = arguments.parse_args()
page = root / "_drafts/progress.html"
class Links(HTMLParser):
    def __init__(self):
        super().__init__()
        self.targets = []
    def handle_starttag(self, tag, attrs):
        for name, value in attrs:
            if name in {"href", "src"} and value:
                self.targets.append(value)
links = Links()
links.feed(page.read_text(encoding="utf-8"))
targets = [(page, target) for target in links.targets]
for document in options.documents:
    document = document.resolve()
    if not document.is_relative_to(root) or not document.is_file():
        raise SystemExit(f"error: document missing or outside workspace: {document}")
    content = re.sub(r"```[\s\S]*?```", "", document.read_text(encoding="utf-8"))
    targets.extend((document, match.group(1).strip("<>")) for match in re.finditer(r"\[[^\]]*\]\(([^)]+)\)", content))
failed = []
checked = 0
for origin, target in targets:
    parsed = urlparse(target)
    windows_path = bool(re.match(r"^[A-Za-z]:[/\\]", target))
    if (parsed.scheme and not windows_path) or not parsed.path:
        continue
    checked += 1
    local = unquote(target.split("#", 1)[0] if windows_path else parsed.path)
    path = (origin.parent / local).resolve()
    if not path.is_file():
        failed.append(str(path))
for target in failed:
    print(f"error: progress target missing: {target}")
print(f"info: progress links checked={checked} failed={len(failed)}")
raise SystemExit(bool(failed))
