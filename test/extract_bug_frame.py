"""Extract the actual HostFrame Bug adoption boundary and lifetime latch."""
from pathlib import Path
import argparse

def body(text, anchor):
    start = text.index(anchor)
    at = text.index('{', start)
    depth = 1
    end = at + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]

p = argparse.ArgumentParser()
p.add_argument('--source-root', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
s = (a.source_root/'src/game/live_host_state/bridge.cpp').read_text(encoding='utf-8')
a.output.mkdir(parents=True, exist_ok=True)
(a.output/'bug_failure_latch.inc').write_text(body(s, 'struct BugFailureLatch {')+';\n', encoding='utf-8')
take = body(s, 'bool Bridge::take(')
start = take.index('if(frame.bugs&&!([&]{apply_timing::Scope measured_group(apply_timing::Part::BugPreflight)')
end = take.index('if(!frame.tutorial||!([&]{apply_timing::Scope measured_group(apply_timing::Part::TutorialPreflight)', start)
(a.output/'bug_adoption_boundary.inc').write_text(take[start:end], encoding='utf-8')
print('Extracted actual Bug preflight/adoption boundary and failure latch')
