from pathlib import Path
import argparse
p=argparse.ArgumentParser();p.add_argument('--root',type=Path,required=True);p.add_argument('--dependencies',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
def body(path,anchor):
    text=path.read_text();start=text.index(anchor);end=text.index('{',start)+1;depth=1
    while depth:depth+=(text[end]=='{')-(text[end]=='}');end+=1
    return text[start:end]
native=a.root/'src/game/tutorial_replication/native.cpp'
observer=a.root/'src/game/tutorial_observer.cpp'
placement=a.dependencies/'src/game/placement_ui_probe/ui_probe.cpp'
(a.output/'tutorial_readers.inc').write_text('\n'.join(body(native,x) for x in ['bool native_state(','bool interval(']))
baseline='\n'.join(body(native,x) for x in ['bool native_state(','bool interval('])
baseline=baseline.replace('\n    mt2mp::game::live_host_state::apply_timing::PageReadScope pages(!share_metadata||!live_host_state::apply_timing::page_read_scope);','').replace('\n    mt2mp::game::live_host_state::apply_timing::PageReadScope pages;','').replace('bool native_state(','bool baseline_native_state(').replace('bool interval(','bool baseline_interval(')
baseline=baseline.replace('bool share_metadata=false','bool=false')
(a.output/'baseline_tutorial_readers.inc').write_text(baseline)
(a.output/'tutorial_strings.inc').write_text(body(native,'bool str('))
(a.output/'observer_copy.inc').write_text(body(observer,'DWORD copy_native('))
(a.output/'observer_member.inc').write_text(body(observer,'bool copy(').replace('override',''))
(a.output/'placement_copy.inc').write_text(body(placement,'std::uint32_t copy_seh(')+'\n'+body(placement,'bool ProcessMemory::copy('))
for anchor in ['bool native_state(','bool interval(']:
    assert 'PageReadScope pages' in body(native,anchor)
for anchor in ['bool history(','DWORD advance_call(','DWORD assign_call(']:
    assert 'PageReadScope' not in body(native,anchor),anchor
assert 'PageReadScope pages;' in body(observer,'bool read(')
