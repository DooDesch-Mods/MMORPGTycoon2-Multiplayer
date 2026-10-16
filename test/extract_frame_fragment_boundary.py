from pathlib import Path
import argparse,hashlib,json
p=argparse.ArgumentParser();p.add_argument('--root',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
source=a.root/'src/game/live_host_state/bridge.cpp';text=source.read_text(encoding='utf-8')
def body(anchor):
 start=text.index(anchor);end=text.index('{',start)+1;depth=1
 while depth:depth+=(text[end]=='{')-(text[end]=='}');end+=1
 return text[start:end]
# These original action-before entrypoints must survive changes to adjacent send().
original_action_before_hashes={'capture_before': 'f990e7d28358f7c4f8d3f85f6f0e990558b0918f62dc19c1955a20417f5273f2', 'publish_before': 'cd37b4bbdf703e1100fb172c8384c3f2bb30f3e5d166e2f964287c1a36c40318'}
for name,expected in original_action_before_hashes.items():
    actual=body('bool Bridge::'+name+'(')
    assert hashlib.sha256(actual.encode('utf-8')).hexdigest()==expected, 'Original Host action-before body changed: '+name
send=body('bool Bridge::send(');route=body('bool Bridge::route_receive(')
assert send.index('fragments::encode(')<send.index('for(uint64_t peer=1;')<send.index('for(const auto& packet:packets)')<send.index('transport_.send(peer,packet,e)')<send.index('host_revision_=')
assert 'n>max_frame_payload' in route and 'fragments_.take(' in route
assert route.index('if(complete.empty())return true;')<route.index('receive(sender,complete.data(),complete.size(),e)')
for method in ['suspend_snapshot','suspend_lobby','rebind_snapshot']:assert 'fragments_.clear()' in body('bool Bridge::'+method+'(')
assert 'fragments_.timely(now,e)' in body('bool Bridge::tick(')
(a.output/'frame_fragment_boundary.json').write_text(json.dumps({'sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'packetization_before_send':True,'complete_before_admission':True,'lifetime_clear':True,'deadline':True},indent=2)+'\n')
# Reuse the actual full Economy fixture; do not recreate its state contract.
fixture=(a.root/'test/test_toon_equipment_value.cpp').read_text(encoding='utf-8');begin=fixture.index('economy::State fixture(){');end=fixture.index('\nbool decode(',begin)
(a.output/'frame_fragment_fixture.inc').write_text(fixture[begin:end]+'\n',encoding='utf-8')

(a.output/"frame_fragment_route.inc").write_text(route+"\n",encoding="utf-8")
