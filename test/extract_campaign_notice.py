"""Exercise the production witness/hook bodies and check their Frame seams."""
import argparse
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--source-root',type=Path,required=True);p.add_argument('--fallback-root',type=Path);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
def read(path):
    target=a.source_root/path
    if not target.exists() and a.fallback_root:target=a.fallback_root/path
    return target.read_text(encoding='utf-8')
def body(text,anchor):
    start=text.index(anchor);end=text.index('{',start)+1;depth=1
    while depth:
        depth+=(text[end]=='{')-(text[end]=='}');end+=1
    return text[start:end]
a.output.mkdir(parents=True,exist_ok=True)
native=read('src/game/marketing_authority/notice_runtime.cpp')
witness=body(native,'    bool observe(')
assert witness.count('result=original(receiver)')==1
assert witness.index('sample(receiver,before')<witness.index('result=original(receiver)')<witness.index('sample(receiver,after')<witness.index('host.append(')
assert 'message::inspect(memory,image,now,created_alert,1,nullptr,selector,payload,e,0)' in witness
(a.output/'campaign_witness.inc').write_text(witness+'\n',encoding='utf-8')
hook=read('src/game/marketing_authority/notice_hook.cpp')
verify=body(hook,'bool verify(')
assert 'body.begin()+5,body.end(),pristine.begin()+5' in verify and 'trampoline!=relocated' in verify
assert 'reinterpret_cast<uintptr_t>(&dispatch)' in verify
assert 'MH_CreateHook' in hook and '0x2330a0' not in hook and '0x2349a0' not in hook
(a.output/'campaign_hook_verify.inc').write_text(verify+'\n',encoding='utf-8')
reader=read('test/test_vip_alert_client_reader.cpp')
(a.output/'campaign_memory.inc').write_text(reader[reader.index('constexpr uintptr_t image='):reader.index('\nunsigned checks=0;')],encoding='utf-8')
bridge=read('src/game/live_host_state/bridge.cpp')
assert 'marketing_port_->acknowledge(sender,revision,e)' in body(bridge,'bool Bridge::route_receive(')
assert 'marketing_port_->sent(frame,e)' in body(bridge,'bool Bridge::send(')
assert 'marketing_port_->admit(frame,queued_?&*queued_:nullptr,e)' in body(bridge,'bool Bridge::receive(')
take=body(bridge,'bool Bridge::take(')
assert take.index('apply_marketing(frame,e)')<take.index('acknowledge_frame(frame,e)')
assert 'marketing_port_->suspend(e)' in bridge and 'marketing_port_->bind(c,b,epoch,e)' in bridge
client=body(read('src/game/marketing_authority/native_port.cpp'),'bool NativePort::apply(')
assert client.count('complete_notices(receipt)')==2
assert '"after notice",false' in client and 'read_current_fence(' in client
assert 'unknown' in body(native,'    bool frame(')
assert 'client.confirm(' in body(native,'bool Runtime::apply(')
for file in ['src/game/marketing_authority/native_port.cpp','src/game/marketing_native_probe/runtime.cpp']:
    assert 'notice_hook::verify(image,e)' in body(read(file),'bool pins(')
invoke=read('src/game/scenario_vip_authority/disconnect_invoke.h')
assert 'int32_t type=9' in invoke and 'f.factory(type,storage)' in invoke
print('PASS Campaign witness original-call ordering, exact hook pins, both pin consumers, Frame ACK/supersession/suspension and Client no-op-plan handoff seams')
