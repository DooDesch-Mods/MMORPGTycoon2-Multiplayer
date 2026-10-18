from pathlib import Path
import argparse
import re

p=argparse.ArgumentParser();p.add_argument('--root',type=Path,required=True);p.add_argument('--dependencies',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
a.output.mkdir(parents=True,exist_ok=True)
def body(path,anchor):
    text=path.read_text(encoding='utf-8');start=text.index(anchor);end=text.index('{',start)+1;depth=1
    while depth:depth+=(text[end]=='{')-(text[end]=='}');end+=1
    return text[start:end]
def emit(name,path,anchors):
    (a.output/name).write_text('\n'.join(body(path,x) for x in anchors)+'\n',encoding='utf-8')
construction=a.root/'src/game/developer_staff/construction_native.cpp'
emit('scoped_actor.inc',construction,['bool actor(const Capture&'])
emit('scoped_jobs.inc',construction,['bool read_jobs('])
emit('scoped_process.inc',a.root/'src/game/developer_staff/native_read.cpp',['bool read_process('])
emit('scoped_staff.inc',a.root/'src/game/tutorial_replication/staff_display_owner.inc',['bool read_owners('])
emit('placement_copy.inc',a.dependencies/'src/game/placement_ui_probe/ui_probe.cpp',['std::uint32_t copy_seh(','bool ProcessMemory::copy('])
text=construction.read_text(encoding='utf-8')
assert text.count('job_census::read(copy,')==1,'all Construction census calls use the pure helper'
assert text.count('read_jobs(')==8,'seven call sites plus helper stay bound'
pattern=re.compile(r'\(\[&\]\{live_host_state::apply_timing::PageReadScope pages;return (region_saved_identity::resolve\(.*?\));\}\(\)\)',re.S)
assert len(pattern.findall(text))==2,'two non-loop Region scans keep their independent pure-read boundary'
assert text.count('PageReadScope pages(!share_metadata);')==1
for anchor in ['bool apply(','bool pair(','bool field(','bool call(','bool retire(']:
    current=body(construction,anchor)
    current=pattern.sub(lambda m:m[1],current)
    assert 'PageReadScope' not in current,anchor+' must not cache across effects'
print('Extracted actual Process/Actor/Staff reader bodies; all original effect boundaries remain unscoped')

emit('scoped_bug_code.inc',a.root/'src/game/bug_authority/hooks.cpp',['bool native_code('])
emit('scoped_developer_pin.inc',a.root/'src/game/developer_staff/action_native.cpp',['bool pin(const PoolState&'])

emit('scoped_bug_tracker.inc',a.root/'src/game/bug_authority/runtime.cpp',['bool tracker_members('])
emit('scoped_job_counter.inc',a.root/'src/game/bug_authority/work_runtime.cpp',['bool read_counter('])

def loop_block(anchor):
    current=body(construction,anchor)
    start=current.index('{live_host_state::apply_timing::PageReadScope actor_pages;')
    end=start+1;depth=1
    while depth:depth+=(current[end]=='{')-(current[end]=='}');end+=1
    return current[start:end]
first=loop_block('bool graph(');second=loop_block('bool capture(const Capture&')
assert 'actor(c,slot,a,e,true)' in first and 'actor(after,slot,second,e,true)' in second
assert text.count('PageReadScope actor_pages;')==2
assert text.count(',e,true)')==2,'only the two pure loops opt in'
(a.output/'scoped_actor_loops.inc').write_text(
    'bool graph_actors(const Capture& c,State& out,std::vector<Actor>& actors,std::string& e)'+
    '{'+first+'return true;}\n'+
    'bool verify_actors(const Capture& after,const std::vector<Actor>& actors,std::string& e)'+
    '{'+second+'return true;}\n',encoding='utf-8')
