from pathlib import Path
import argparse,hashlib,json
p=argparse.ArgumentParser();p.add_argument('--root',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
base=a.root/'src/game/subscriber_lifecycle'
header=(base/'npc_reference_pins.h').read_text(encoding='utf-8')
owners=(a.root/'src/game/npc_host_state_binding.h').read_text(encoding='utf-8')
quest_owners=owners[owners.index('inline bool reject('):owners.index('template<class Operations,class Resolve>')]
(a.output/'subscriber_quest_owners.inc').write_text('namespace mt2mp::game::npc_probe::action_runtime::host_binding {\n'+quest_owners+'}\n',encoding='utf-8')
body=header[header.index('namespace mt2mp::game::subscriber_npc_references {'):]
(a.output/'subscriber_npc_pins.inc').write_text(body,encoding='utf-8')
capture=(base/'runtime_capture.cpp').read_text(encoding='utf-8');append=(base/'runtime_append.cpp').read_text(encoding='utf-8')
assert capture.index('npc_pins.prepare(')<capture.index('callbacks.body =')<capture.index('local::with_save_scope(')
assert 'before.world.map), &building_pins, &npc_pins}' in capture
assert 'row["portable_owner"]["npc_key"]=key.npc_key;' in capture
assert 'building_pins.verify_used(e)&&npc_pins.verify_used(e)' in capture
assert '!building_pins.verify_used(cause)||!npc_pins.verify_used(cause)' in capture
start=append.index('bool apply_impl(');apply=append[start:]
assert apply.index('work.npc_pins.prepare(')<apply.index('local::with_save_scope(')
assert '!work.building_pins.verify_used(e)||!work.npc_pins.verify_used(e)' in apply
assert append.count('&w.building_pins,&w.npc_pins}')==2
assert '&&w.building_pins.verify_used(e)&&w.npc_pins.verify_used(e)' in append
assert '+ref.key.building_key+ref.key.npc_key+' in append
fresh=append[append.index('bool fresh_link('):append.index('bool toon_array(')]
assert 'stage::resolve_reference(memory,world,w.keys[i].first,p,e)' in fresh
assert 'p==link.object' in fresh
(a.output/'subscriber_npc_wiring.json').write_text(json.dumps({p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in [base/'npc_reference_pins.h',base/'runtime_capture.cpp',base/'runtime_append.cpp']},indent=2)+'\n')

assert '"shared_quest"' in capture
assert 'row["portable_owner"]["sequence_id"]=key.sequence_id;' in capture
assert 'r.key.kind==stage::ReferenceKind::SharedNPC||r.key.kind==stage::ReferenceKind::SharedQuest' in apply
assert 'std::to_string(ref.key.sequence_id)' in append
assert 'used_quests_' in header and 'fresh_quest' in header

body=append[append.index('bool body_inner('):append.index('bool body(void*')]
assert body.index('context_destroy,context,dtor')<body.index('verify_quest_references(e)')<body.index('local::Census after;')
assert 'w.unknown=true;return false;' in body[body.index('verify_quest_references(e)'):body.index('local::Census after;')]
