"""Extract actual portable VIP Bridge methods and assert their effect boundaries."""
import argparse
from pathlib import Path

def body(text,anchor):
    begin=text.index(anchor)
    end=text.index('{',begin)+1
    depth=1
    while depth:
        depth+=(text[end]=='{')-(text[end]=='}')
        end+=1
    return text[begin:end]

p=argparse.ArgumentParser()
p.add_argument('--source-root',type=Path,required=True)
p.add_argument('--output',type=Path,required=True)
a=p.parse_args()
s=(a.source_root/'src/game/live_host_state/bridge.cpp').read_text(encoding='utf-8')
h=(a.source_root/'src/game/live_host_state/bridge.h').read_text(encoding='utf-8')
methods=['bind_vip','capture_vips','sent_vips','acknowledge_vips','admit_vips','prepare_vips','apply_vips','suspend_vips','reset_vips','suspend_lobby']
actual='\n'.join(body(s,'bool Bridge::'+name+'(') for name in methods)
a.output.mkdir(parents=True,exist_ok=True)
(a.output/'vip_bridge.h').write_text(h.replace('class Bridge {','struct Bridge {',1),encoding='utf-8')
(a.output/'vip_bridge_methods.inc').write_text(actual+'\n',encoding='utf-8')
(a.output/'vip_failure_latch.inc').write_text(body(s,'struct VipFailureLatch {')+';\n',encoding='utf-8')

capture=body(s,'bool Bridge::capture(')
assert capture.index('capture_vips(out,e)')<capture.index('if(!valid(out,e))')<capture.index('day_->capture(')
send=body(s,'bool Bridge::send(')
assert send.index('transport_.send(peer,packet,e)')<send.index('sent_vips(frame,e)')<send.index('host_revision_=frame.revision')
receive=body(s,'bool Bridge::receive(')
assert 'bool(frame.vips)!=bool(vip_port_)' in receive
assert receive.index('const auto gate=admit(')<receive.index('admit_vips(frame,e)')<receive.index('queued_=frame')
take=body(s,'bool Bridge::take(')
assert take.index('prepare_vips(frame,vip_work,e)')<take.index('port_.read(before,e)')
assert take.index('economy_->after_day(')<take.index('tutorial_replication::apply(')<take.index('apply_vips(frame,vip_work,vip_attempted,e)')<take.index('acknowledge_frame(frame,e)')<take.index('vip_completed=true;')
route=body(s,'bool Bridge::route_receive(')
assert route.index('apply_acks_.validate(')<route.index('economy_->acknowledge_subscribers(')<route.index('acknowledge_vips(sender,revision,e)')<route.index('apply_acks_.acknowledge(')
tick=body(s,'bool Bridge::tick(')
assert 'last_->vips==frame.vips' in tick
for name in ['suspend_snapshot','suspend_lobby']:
    value=body(s,'bool Bridge::'+name+'(')
    assert value.index('suspend_vips(e)')<value.index('suspended_=')
assert 'reset_vips(c,b,epoch,e)' in body(s,'bool Bridge::rebind_snapshot(')
print('PASS actual VIP capture/send/ACK/supersession/effect/lifetime wiring boundaries')
