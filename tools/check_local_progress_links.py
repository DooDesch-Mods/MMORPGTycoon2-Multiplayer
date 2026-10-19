"""Check affected local progress and handoff references without opening a browser."""
from pathlib import Path
from html.parser import HTMLParser
import re,json
ROOT=Path(__file__).resolve().parents[1]
class Links(HTMLParser):
    def __init__(self):super().__init__();self.links=[]
    def handle_starttag(self,tag,attrs):
        for name,value in attrs:
            if name in {'src','href'}:self.links.append(value)
checks=[]
for name in ('_drafts/progress.html','_drafts/implementation_state.md','_drafts/action_matrix.md','_drafts/time_mouse_two.md','_drafts/ui_remove_two.md','_drafts/placement_phases.md','_drafts/original_build_two.md','_drafts/normal_build_two.md','_drafts/client_move_two.md','_drafts/host_move_two.md','_drafts/normal_move_two.md'):
    path=ROOT/name;text=path.read_text(encoding='utf-8')
    if path.suffix=='.html':
        parser=Links();parser.feed(text);links=parser.links
    else:links=re.findall(r'\]\(([^)]+)\)',text)
    local=[x for x in links if not x.startswith(('http:','https:','#'))]
    missing=[x for x in local if not (path.parent/x.split('#')[0]).exists()]
    checks.append({'file':name,'local_links':len(local),'missing':missing})
print(json.dumps(checks,indent=2))
raise SystemExit(1 if any(item['missing'] for item in checks) else 0)
