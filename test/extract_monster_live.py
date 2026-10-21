"""Extract production Monster frame gates and callback bodies for bounded tests."""
import argparse,hashlib,json
from pathlib import Path
def body(text,anchor):
 start=text.index(anchor);end=text.index('{',start)+1;depth=1
 while depth:
  depth+=(text[end]=='{')-(text[end]=='}');end+=1
 return text[start:end]
p=argparse.ArgumentParser();p.add_argument('--source-root',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
a.output.mkdir(parents=True,exist_ok=True)
def write(name,text):
 path=a.output/name
 if not path.exists() or path.read_text()!=text:path.write_text(text,encoding='utf-8',newline='\n')
files={}
def source(name):
 data=(a.source_root/name).read_bytes();files[name]=hashlib.sha256(data).hexdigest();return data.decode('utf-8-sig').replace('\r\n','\n')
s=source('src/game/live_host_state/bridge.cpp');h=source('src/game/live_host_state/bridge.h')
write('monster_live_bridge.h',h.replace('class Bridge {','struct Bridge {',1))
write('monster_frame_apply.inc',body(s,'bool Bridge::apply_monsters(')+'\n')
take=body(s,'bool Bridge::take(')
assert take.index('monster_port_->preflight(')<take.index('economy_->before_day(')
assert take.index('economy_->after_day(')<take.index('apply_marketing(frame,e)')<take.index('apply_vips(frame,vip_work,vip_attempted,e)')<take.index('apply_monsters(frame,e)')<take.index('acknowledge_frame(frame,e)')<take.index('last_=frame')
capture=body(s,'bool Bridge::capture(')
assert capture.index('day_->capture(')<capture.index('economy_->capture(')<capture.index('monster_port_->capture(')
assert 'captured_day_->frame=out' in capture and 'captured_economy_->day.frame=out' in capture
assert 'last_->monsters==frame.monsters' in body(s,'bool Bridge::tick(')
assert 'bool(frame.monsters)!=bool(monster_port_)' in body(s,'bool Bridge::receive(')
assert 'monster_port_->after_action(context_,binding_,e)' in body(s,'bool Bridge::after_action(')
for anchor in ['bool Bridge::suspend_snapshot(', 'bool Bridge::suspend_lobby(']:assert 'monster_port_->suspend(' in body(s,anchor)
assert 'monster_port_->bind(c,b,epoch,e)' in body(s,'bool Bridge::rebind_snapshot(')
n=source('src/game/monster_live_runtime.inc')
write('monster_live_callbacks.inc',body(n,'void live_update_entry(uintptr_t object')+'\n'+body(n,'float live_adjust_entry(uintptr_t object')+'\n')
write('monster_live_identity_bodies.inc','\n'.join(body(n,x) for x in ['bool world(Memory&','bool full(Memory&','bool read(ml::State&'])+'\n')
reader=source('src/game/object_identity/remove_runtime/copy/world_prop_reader.cpp')
write('monster_live_world_reader.inc','\n'.join(body(reader,x) for x in ['bool fail(std::string& e,const char*','struct Busy {','template<class T>bool at(','bool vt(']).replace('~Busy(){b=false;}}','~Busy(){b=false;}};')+'\n'+body(reader,'bool Reader::current(')+'\n')
scope=source('src/game/subscriber_lifecycle/save_scope.cpp')
write('monster_live_save_scope.inc','\n'.join(body(scope,x) for x in ['bool callback(','bool begin_save_scope(','bool finish_save_scope(','bool with_save_scope('])+'\n')
write('monster_live_lifecycle_bodies.inc','\n'.join(body(n,x)+(';' if x.startswith('struct ') else '') for x in ['struct Membership {','bool membership(Memory&','bool expire_proxy(Memory&','bool lifecycle_scalars(Memory&','void trace(const ml::State&','bool row(Memory&','bool set(const ml::Row&'])+'\n')
write('monster_live_animation_row.inc',body(n,'bool row(Memory&')+'\n')
write('monster_live_present.inc',body(n,'bool present(uintptr_t object')+'\n')
write('monster_live_classify.inc',body(n,'bool classify(uintptr_t object').replace('bool classify(','bool MonsterLivePort::classify(',1)+'\n')
for anchor in ['bool set(const ml::Row&','bool present(uintptr_t object','bool row(Memory&','bool read(ml::State&']:
 extracted=body(n,anchor)
 for forbidden in ['0x3a0350','0x3b9eb0','0x39afe0','0x3be180','0x3bc420','0x39f920']:assert forbidden not in extracted,(anchor,forbidden)
set_body=body(n,'bool set(const ml::Row&')
assert set_body.index('attempted=true;const auto seh=live_transform(')<set_body.index('ml::clear_spatial_dirty(')
assert set_body.index('for(bool playing:{false,true})')<set_body.index('live_animation_set(')
for check in ['prior.state.global_random!=result.state.global_random','prior.state.region_random!=result.state.region_random','prior.undo!=result.undo','prior.release_edits!=result.release_edits','ledger(prior.money)!=ledger(result.money)','!ml::same_applied(actual,target)','mr::unchanged_others(']:assert check in set_body,check
animation=body(n,'uint32_t live_animation_set(')
assert animation.index('0x28730')<animation.index('0x237a0')<animation.index('0x23770')<animation.index('0x237e0')
presentation=body(n,'uint32_t live_present(')
assert presentation.index('0x39d670')<presentation.index('0x287d0')<presentation.index('0x28970')<presentation.index('0x21df0')
for call in ['0x3a0350','0x3b9eb0','0x39afe0','0x3c0750']:assert call not in presentation
assert '_InterlockedCompareExchange8' in body(n,'uint32_t live_byte(')
full=body(n,'bool full(Memory&')
for guard in ['0x16c0770','0x18,uid','0x170,root','0x178,region','0x988,manager','0x80,owner','0x8d0,zone','0x8d8,proxy','lookup(p.api.find,uid,found)','identity::registry.resolve(','identity::registry.lookup(','q::unchanged(']:assert guard in full,guard
read=body(n,'bool read(ml::State&')
assert 'out.rows.size()!=expected.size()' in read and 'if(zone)continue;if(proxy)' in read and 'identity::registry.enumerate(' in read
catalog=source('src/hooks/framework_catalog.inc')
for route in ['mp.monster-entity-update.3c0750','mp.character-adjust-attribute.39fb80']:assert catalog.count(route)==1,route
runtime=source('src/game/typed_action_runtime.cpp')
assert 'bind_monsters(npc_probe::monster_action_runtime::live_port(),error)' in runtime
write('monster_live_idle_binding.inc',body(runtime,'bool read_binding(')+'\n'+body(runtime,'bool network_binding(')+'\n')
bound=source('src/game/npc_action_runtime.inc')
write('monster_live_world_binding.inc',body(bound,'bool bound(const host::WorldState&')+'\n')
pending=[]
for anchor in ['bool world(Memory&','bool capture(const a::Context&','bool preflight(const a::Context&','bool apply(const a::Context&','bool after_action(const a::Context&']:
 extracted=body(n,anchor)
 assert 'network_binding(' not in extracted
 start=extracted.index('(');name=extracted[5:start]
 extracted=extracted[:5]+'MonsterLivePort::'+extracted[5:]
 extracted=extracted.replace('override','').replace(',bool idle=true',',bool idle')
 pending.append(extracted)
write('monster_live_pending_bodies.inc','\n'.join(pending)+'\n')
write('extraction.json',json.dumps({'sources':files,'checks':'actual frame preflight/final apply/ACK, lifecycle, native guards and presentation ordering'},indent=2)+'\n')
print('PASS actual Monster Frame, lifecycle, native guard, byte-CAS and presentation order checks')
