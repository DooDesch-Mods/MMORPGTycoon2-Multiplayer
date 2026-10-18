"""Extract the production ACK gate and retain its actual ordering checks."""
import argparse
from pathlib import Path
def body(text,anchor):
    start=text.index(anchor);end=text.index('{',start)+1;depth=1
    while depth:
        depth+=(text[end]=='{')-(text[end]=='}');end+=1
    return text[start:end]
p=argparse.ArgumentParser();p.add_argument('--source-root',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
s=(a.source_root/'src/game/live_host_state/bridge.cpp').read_text(encoding='utf-8')
h=(a.source_root/'src/game/live_host_state/bridge.h').read_text(encoding='utf-8')
a.output.mkdir(parents=True,exist_ok=True)
(a.output/'marketing_bridge.h').write_text(h.replace('class Bridge {','struct Bridge {',1),encoding='utf-8')
(a.output/'marketing_apply.inc').write_text(body(s,'bool Bridge::apply_marketing(')+'\n',encoding='utf-8')
take=body(s,'bool Bridge::take(')
assert take.index('economy_->after_day(')<take.index('apply_marketing(frame,e)')<take.index('apply_vips(frame,vip_work,vip_attempted,e)')<take.index('acknowledge_frame(frame,e)')<take.index('last_=frame')
capture=body(s,'bool Bridge::capture(')
assert capture.index('marketing_port_->capture(')<capture.index('day_->capture(')
assert 'bool(frame.marketing)!=bool(marketing_port_)' in body(s,'bool Bridge::receive(')
assert 'last_->marketing==frame.marketing' in body(s,'bool Bridge::tick(')
n=(a.source_root/'src/game/marketing_authority/native_port.cpp').read_text(encoding='utf-8')
host=body(n,'bool NativePort::capture(')
for call in ['read(c,b,frame,false,observed,e)','pins(observed.image,e)','valid(State{observed.snapshot.values}','host_frame_matches(frame,observed.finance,e)']:
    assert call in host,call
for mutation in ['execute(', 'with_save_scope(', 'capture_original(', 'invoke(', 'call(', 'host_body(']:
    assert mutation not in host,mutation
assert 'bool host_body(' not in n and 'capture_original(' not in n
native_read=body(n,'bool read(const action_session::Context&')
assert native_read.count('read_once(')==2 and 'same_guards(a,p,"double read",false,e)' in native_read
client=body(n,'bool NativePort::apply(')
assert 'execute(w,e)' in client and client.count('read_current_fence(')==3
scope=body(n,'bool execute(')
assert 'with_save_scope(' in scope and 'restore_original_pause' in scope
assert 'same_guards(w.before,p,"restored",false,e)' in scope
assert 'guard_before' in scope and 'last_guard_observation' in body(n,'bool unchanged(')
print('PASS actual Marketing capture, Day/Economy apply, ACK and unchanged publication order')
print('PASS actual Host capture read-only body; double-read/Frame guards and Client SaveScope/fence/pause path retained')

(a.output/'tutorial_publication.inc').write_text(body(s,'bool same_tutorial_publication(')+'\n',encoding='utf-8')
tick=body(s,'bool Bridge::tick(')
assert tick.count('same_tutorial_publication(last_->tutorial,frame.tutorial)')==1
assert s.count('same_tutorial_publication(')==2
assert tick.index('capture(action,frame,e)')<tick.index('same_tutorial_publication(')<tick.index('return send(frame,e)')
for guard in ['last_day_sequence_==day_->sequence()', 'captured_economy_->subscribers==last_economy_->subscribers',
              'captured_economy_->loans==last_economy_->loans', 'captured_economy_->offers==last_economy_->offers',
              'captured_economy_->npcs==last_economy_->npcs', 'captured_economy_->additions==last_economy_->additions',
              'last_->marketing==frame.marketing', 'last_->vips==frame.vips', 'last_->bugs==frame.bugs',
              'last_->developers==frame.developers', 'last_->network==frame.network', 'last_->values==frame.values',
              'last_->selected==frame.selected', 'last_->pause==frame.pause',
              'last_->base_action_revision==action', 'last_->time_revision==frame.time_revision']:
    assert guard in tick,guard
