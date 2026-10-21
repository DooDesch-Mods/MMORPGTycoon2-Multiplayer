"""Extract actual Leave-only Bridge/Fence bodies and expose fields only in tests."""
import argparse
from pathlib import Path

def part(text,begin,end):
    if text.count(begin)!=1 or text.count(end)!=1:
        raise SystemExit('locked Leave extraction boundary changed: '+begin)
    return text[text.index(begin):text.index(end)]

def extract(root,output):
    folder=root/'src/game/live_host_state'
    bridge=(folder/'bridge.cpp').read_text(encoding='utf-8-sig')
    fence=(folder/'economic_fence.cpp').read_text(encoding='utf-8-sig')
    bodies={
        'bridge_leave.inc':part(bridge,'bool Bridge::suspend_snapshot(','\n}\n\nnamespace mt2mp::game::live_host_state {\nbool Bridge::after_action'),
        'fence_suspend.inc':part(fence,'bool EconomicFence::suspend_snapshot(','bool EconomicFence::rebind_snapshot('),
        'fence_retire.inc':part(fence,'bool EconomicFence::retire_lobby(','bool retire_lobby_fence('),
    }
    def body(anchor,text=bridge):
        begin=text.index(anchor);end=text.index('{',begin)+1;depth=1
        while depth:
            depth+=(text[end]=='{')-(text[end]=='}');end+=1
        return text[begin:end]
    cash=(folder/'cash_contract.cpp').read_text(encoding='utf-8-sig')
    bodies['fence_cash.inc']='\n'.join([
        body('bool brush_cash_shape(',cash),
        body('bool brush_cash_prefix(',cash),
        body('bool brush_cash_progress(',cash),
        body('bool brush_cash_complete(',cash),
        body('bool EconomicFence::allow(',fence),
        body('bool EconomicFence::end_action(',fence),
        body('void EconomicFence::original_failed(',fence),
        body('bool predicted_cash(',(folder/'cash_contract.cpp').read_text(encoding='utf-8-sig')),
        body('action_session::Ledger ledger(',(folder/'state.cpp').read_text(encoding='utf-8-sig')),
    ])+'\n'
    bodies['bridge_leave.inc']+='\n'+body('bool Bridge::suspend_vips(')+'\n'+body('bool Bridge::reset_vips(')+'\n'
    for name,classname in [('bridge.h','Bridge'),('economic_fence.h','EconomicFence')]:
        text=(folder/name).read_text(encoding='utf-8-sig')
        if text.count('class '+classname)!=1:raise SystemExit('locked Leave test header boundary changed: '+name)
        bodies[name]=text.replace('class '+classname,'struct '+classname,1)
    assert 'action_cash_ledger_=result.evidence.before;' in body('bool EconomicFence::begin_action(',fence)
    output.mkdir(parents=True,exist_ok=True)
    for name,body in bodies.items():(output/name).write_bytes(body.encode('utf-8'))

if __name__=='__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('--source-root',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();extract(args.source_root,args.output)
