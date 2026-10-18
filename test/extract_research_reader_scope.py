from pathlib import Path
import argparse
p=argparse.ArgumentParser();p.add_argument('--root',type=Path,required=True);p.add_argument('--dependencies',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
def body(path,anchor):
    text=path.read_text(encoding='utf-8');start=text.index(anchor);end=text.index('{',start)+1;depth=1
    while depth:depth+=(text[end]=='{')-(text[end]=='}');end+=1
    return text[start:end]
dev=a.root/'src/game/tutorial_development.cpp'
weapons=a.root/'src/game/toon_weapon_visual/native.cpp'
copy=a.dependencies/'src/game/placement_ui_probe/ui_probe.cpp'
(a.output/'reader_copy.inc').write_text(body(copy,'std::uint32_t copy_seh(')+'\n'+body(copy,'bool ProcessMemory::copy(')+'\n')
(a.output/'development_entries.inc').write_text('\n'.join(body(dev,x) for x in ['bool read(nlohmann::json&','bool read_startingpoint(','bool read_research('])+'\n')
(a.output/'weapon_reader_entries.inc').write_text('\n'.join(body(weapons,x).replace('override','') for x in ['bool pin(uintptr_t','bool plan(uintptr_t','bool matches(const Plan&'])+'\n')
for name in ['scene','remove','cache','attach','matrix']:
    assert 'PageReadScope' not in body(weapons,'bool '+name+'('),name
assert 'PageReadScope' not in body(weapons,'bool refresh(')
getter=body(dev,'bool research_flags(')
assert 'PageReadScope getter_scope(live_host_state::apply_timing::PageBoundary::OriginalCall);' in getter
assert getter.index('PageReadScope getter_scope')<getter.index('research_call(')
assert 'PageReadScope pages;' in body(dev,'bool read(nlohmann::json&')
for anchor in ['bool read_startingpoint(','bool read_research(']:
    assert 'PageReadScope pages(!share_metadata||!live_host_state::apply_timing::page_read_scope);' in body(dev,anchor)

staff=a.root/'src/game/tutorial_replication/staff_display_owner.inc'
(a.output/'staff_owner_reader.inc').write_text(body(staff,'bool read_owners(')+'\n')

native=a.root/'src/game/tutorial_replication/native.cpp'
research=a.root/'src/game/tutorial_replication/research_native.inc'
starting=a.root/'src/game/tutorial_replication/startingpoint_native.inc'
assert 'read_startingpoint(snapshot,diagnostic,e,true,true)' in body(native,'bool native_state(')
assert 'display_native::capture(c,b,snapshot,out.staff_display,e,true)' in body(native,'bool native_state(')
pre=body(native,'static bool preflight_local(')
assert 'PageReadScope pages;' in pre and 'native_state(c,b,local,native,e,true)' in pre
assert '"startingpoint",true)' in pre
assert 'calls.share_reader_metadata=true;' in body(research,'bool research_preflight(')
assert 'share_reader_metadata' not in body(research,'bool research_apply(')
assert 'bool share_reader_metadata=false;' in starting.read_text()
assert 'read_research(key,out,diagnostic,e,creating||key=="bridges",share_reader_metadata)' in starting.read_text()
assert 'read_startingpoint(first,diagnostic,e,false,true)' in body(research,'bool research_capture(')
assert 'read_research("bridges",current,diagnostic,e,true,true)' in body(research,'bool research_capture(')
assert 'display_native::capture(c,b,current,out.staff_display,e,true)' in body(research,'bool research_capture(')
admit=body(staff,'bool admit(')
assert 'read_research(key,s,d,e,true,!install&&share_metadata)' in admit
assert 'read_owners(s,owners,e,!install&&share_metadata)' in admit
assert 'PageReadScope' not in body(native,'bool apply(')

day=a.root/'src/game/live_host_state/day_rollover/native_day.cpp'
(a.output/'day_reader_entries.inc').write_text('\n'.join(body(day,x) for x in ['bool NativePort::read_once(','bool NativePort::read(NativeHistory&','bool NativePort::read_quest_held('])+'\n')
