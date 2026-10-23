from pathlib import Path
import argparse,hashlib,json
p=argparse.ArgumentParser();p.add_argument('--source-root',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
source=a.source_root/'src/game/management_live_port.inc';text=source.read_text()
start='uintptr_t scalar_address(';end='uint32_t slots_once('
assert text.count(start)==1 and text.count(end)==1,'Management actual native body boundary changed'
body=text[text.index(start):text.index(end)]
a.output.mkdir(parents=True,exist_ok=True);target=a.output/'management_scalar_body.inc';target.write_text(body)
(a.output/'management_scalar_body.json').write_text(json.dumps({'source':str(source),'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'extracted_sha256':hashlib.sha256(target.read_bytes()).hexdigest()},indent=2)+'\n')

# Exercise the production outer read and its nested Thought sampling boundary.
for name,start,end in [('management_calendar_actual.inc','bool calendar_sample(','bool primary_once('),
                       ('management_primary_actual.inc','bool primary_read(','#include "gm_ticket_live.inc"')]:
    assert text.count(start)==1 and text.count(end)==1,'Management paired sample boundary changed'
    (a.output/name).write_text(text[text.index(start):text.index(end)],encoding='utf-8')
primary=text[text.index('bool primary_once('):text.index('bool primary_read(')]
assert primary.count('calendar_sample(r,context,binding,local.pin,out,e,&thoughts,planning)')==1
assert 'thought_frame_native::Snapshot& thoughts' in primary
assert text.count('primary_read(*r,context,binding,local,before,e,true)')==1
assert 'primary_read(*r,context,binding,final,actual,e)' in text
assert 'calendar_sample(*r,context,binding,pin,*frame.management,error)' in text
assert 'primary_read(r,context,binding,local,before,error)' in text # GM original refresh stays full.
assert 'primary_read(*r,context,binding,local,state,e)' in text # Bind stays full.
assert 'primary_read(*r,context,binding,local,out,e)' in text # Host capture stays full.
analytics_start=primary.index('    if(!planning){\n    analytics_state::State')
analytics_end=primary.index('    if(!calendar_sample(')
(a.output/'management_planning_analytics_actual.inc').write_text(primary[analytics_start:analytics_end],encoding='utf-8')
tail=primary[primary.index('    {std::optional<live::apply_timing::Scope> measured_services;'):]
(a.output/'management_planning_tail_actual.inc').write_text(tail[:tail.rfind('}')],encoding='utf-8')
prior=primary.replace('std::string& e,bool planning){','std::string& e){',1)
prior=prior.replace('    if(!planning){\n    analytics_state::State','    analytics_state::State',1)
prior=prior.replace('    out.analytics=std::move(analytics);\n    }','    out.analytics=std::move(analytics);',1)
prior=prior.replace('&thoughts,planning)', '&thoughts)',1)
assert hashlib.sha256(prior.encode()).hexdigest()=='fb1ea8f441331e0388d0d3fa7c0147eb7a48b72162842d954f056df0435e0ddf', 'Planning changed original roots, Service, Project or validation body'
apply=text[text.index('bool LivePort::apply('):text.index('bool LivePort::sent(')]
failed=apply[apply.index('    auto failed='):apply.index('    if(!gm_prepare_pool(')]
(a.output/'management_apply_failure_actual.inc').write_text(failed+'    return failed(e);\n',encoding='utf-8')
for component in ['thought_frame_native::adopt','analytics_native::adopt','tutorial_replication::apply_intro','dungeon_gizmo::apply_live']:
    assert apply.count(component)==1, 'Fresh child adoption must remain'
assert 'if(!intro_okay)return failed(e);' in apply and 'if(!gizmo_okay)return failed(e);' in apply
assert 'return thought_frame_native::adopt(*r,context,binding,*frame.management->thoughts,receipt,e);}()))return failed(e);' in apply
assert 'return analytics_native::adopt(*r,context,binding,*frame.management->analytics,receipt,e);}()))return failed(e);' in apply
assert 'before.thoughts?"complete":"planning_with_calendar_demands"' in apply

analytics=a.source_root/'src/game/analytics_native.inc'
native=analytics.read_text(encoding='utf-8')
start='enum class Op {';end='uint32_t invoke('
assert native.count(start)==1 and native.count(end)==1,'Analytics native call boundary changed'
(a.output/'analytics_call_types.inc').write_text(native[native.index(start):native.index(end)],encoding='utf-8')
start='bool adopt(Runtime&';assert native.count(start)==1,'Analytics actual Apply boundary changed'
(a.output/'analytics_adopt_actual.inc').write_text(native[native.index(start):native.rfind('\n}')],encoding='utf-8')
(a.output/'analytics_actual.json').write_text(json.dumps({'source':str(analytics),'sha256':hashlib.sha256(analytics.read_bytes()).hexdigest(),'functions':['adopt'],'native_calls':'fixture original calls; game is not invoked'},indent=2)+'\n',encoding='utf-8')

demands=a.source_root/'src/game/subscriber_demand_native.inc'
native=demands.read_text(encoding='utf-8')
start='uint32_t compare_pointer('
assert native.count(start)==1 and native.count('bool apply(Runtime&')==1,'Subscriber Demand actual Apply boundary changed'
(a.output/'subscriber_demand_apply_actual.inc').write_text(native[native.index(start):native.rfind('\n}')],encoding='utf-8')
(a.output/'subscriber_demand_actual.json').write_text(json.dumps({'source':str(demands),'sha256':hashlib.sha256(demands.read_bytes()).hexdigest(),'functions':['compare_pointer','apply'],'native_calls':'fixture canonical capture; actual native pointer CAS; game is not invoked'},indent=2)+'\n',encoding='utf-8')

thoughts=a.source_root/'src/game/toon_thought_lifetime.inc'
native=thoughts.read_text(encoding='utf-8')
start='void destroy(void* receiver){';end='bool enable(Runtime&'
assert native.count(start)==1 and native.count(end)==1,'Thought actual destruction boundary changed'
(a.output/'thought_destroy_actual.inc').write_text(native[native.index(start):native.index(end)],encoding='utf-8')
(a.output/'thought_destroy_actual.json').write_text(json.dumps({'source':str(thoughts),'sha256':hashlib.sha256(thoughts.read_bytes()).hexdigest(),'functions':['destroy'],'native_calls':'fixture native owner/remover/destructor; game is not invoked'},indent=2)+'\n',encoding='utf-8')

thought_frame=a.source_root/'src/game/toon_thought_frame.inc'
native=thought_frame.read_text(encoding='utf-8')
start='enum class Op{';end='uint32_t invoke('
assert native.count(start)==1 and native.count(end)==1,'Thought call boundary changed'
(a.output/'thought_frame_calls.inc').write_text(native[native.index(start):native.index(end)],encoding='utf-8')
start='inline bool compatible(';assert native.count(start)==1,'Thought actual adoption boundary changed'
(a.output/'thought_frame_adopt_actual.inc').write_text(native[native.index(start):native.rfind('\n}')],encoding='utf-8')
(a.output/'thought_frame_actual.json').write_text(json.dumps({'source':str(thought_frame),'sha256':hashlib.sha256(thought_frame.read_bytes()).hexdigest(),'functions':['compatible','resize','construct','append','reorder','adopt'],'native_calls':'fixture originals; actual production algorithm and typed memory reader; game is not invoked'},indent=2)+'\n',encoding='utf-8')

gizmos=a.source_root/'src/game/dungeon_gizmo_live_native.inc'
native=gizmos.read_text(encoding='utf-8')
start='uint32_t live_cas(';end='uint32_t live_on_map('
assert native.count(start)==1 and native.count(end)==1,'Gizmo actual scalar CAS boundary changed'
(a.output/'gizmo_live_cas_actual.inc').write_text(native[native.index(start):native.index(end)],encoding='utf-8')
start='struct LiveSample {';end='\n}\nbool capture_live('
assert native.count(start)==1 and native.count(end)==1,'Gizmo actual current read boundary changed'
(a.output/'gizmo_live_read_actual.inc').write_text(native[native.index(start):native.index(end)],encoding='utf-8')
start='bool capture_live(';end='bool apply_live('
assert native.count(start)==1 and native.count(end)==1,'Gizmo actual capture boundary changed'
(a.output/'gizmo_live_capture_actual.inc').write_text(native[native.index(start):native.index(end)],encoding='utf-8')
start='bool apply_live('
assert native.count(start)==1,'Gizmo actual full adoption boundary changed'
gizmo_apply=native[native.index(start):native.rfind('\n}')]
(a.output/'gizmo_live_apply_actual.inc').write_text(gizmo_apply,encoding='utf-8')
empty_fast_path='    // The fresh paired census proves no Scene exists on either side; no adoption can run.\n    if(before.value.empty()&&target.empty()){receipt.confirmed=true;error.clear();return true;}\n'
assert gizmo_apply.count(empty_fast_path)==1,'Gizmo empty-only admission changed'
gizmo_baseline=gizmo_apply.replace(empty_fast_path,'')
assert hashlib.sha256(gizmo_baseline.encode()).hexdigest()=='81a140976151e67aefe46c20f9a2702082db40f1a4d0b0d2bd102da8dcbabf81','Gizmo adoption changed beyond empty-only fast path'
(a.output/'gizmo_live_apply_baseline.inc').write_text(gizmo_baseline.replace('bool apply_live(','bool apply_live_baseline(',1),encoding='utf-8')
(a.output/'gizmo_live_actual.json').write_text(json.dumps({'source':str(gizmos),'sha256':hashlib.sha256(gizmos.read_bytes()).hexdigest(),'functions':['live_cas','live_scope','live_once','live_sample','capture_live','apply_live'],'native_calls':'actual owned-memory CAS and actual reader; original render calls use controlled fixture ports; no game invoked'},indent=2)+'\n',encoding='utf-8')

binding_source=a.source_root/'src/game/typed_action_runtime.cpp'
native=binding_source.read_text(encoding='utf-8')
start='bool read_binding(';end='bool observe_startingpoint_submission('
assert native.count(start)==1 and native.count(end)==1,'Controller current/admission binding boundary changed'
(a.output/'controller_binding_actual.inc').write_text(native[native.index(start):native.index(end)],encoding='utf-8')
(a.output/'controller_binding_actual.json').write_text(json.dumps({'source':str(binding_source),'sha256':hashlib.sha256(binding_source.read_bytes()).hexdigest(),'functions':['read_binding','network_binding'],'native_calls':'controlled Controller/Session/Engine fixture; actual binding APIs; no game invoked'},indent=2)+'\n',encoding='utf-8')

uptime=a.source_root/'src/game/building_uptime_native.inc'
native=uptime.read_text(encoding='utf-8')
start='bool current_owner(';end='void dispatch(Runtime*'
assert native.count(start)==1 and native.count(end)==1,'Building uptime actual current owner boundary changed'
(a.output/'building_uptime_owner_actual.inc').write_text(native[native.index(start):native.index(end)],encoding='utf-8')
start='void dispatch(Runtime*';end='void entry('
assert native.count(start)==1 and native.count(end)==1,'Building uptime actual hook dispatch boundary changed'
(a.output/'building_uptime_dispatch_actual.inc').write_text(native[native.index(start):native.index(end)],encoding='utf-8')
