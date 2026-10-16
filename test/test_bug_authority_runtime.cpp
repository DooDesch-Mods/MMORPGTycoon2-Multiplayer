#include "bug_authority/runtime_internal.h"
#include "remove_diagnostics.h"
#include "npc_probe.h"
#include "developer_staff/job_census.h"
#include "mt2mp/core/logger.h"
#include <Windows.h>
#include <cstring>
#include <iostream>
#include <map>
#include <stdexcept>
namespace bugs=mt2mp::game::bug_authority;
namespace create=mt2mp::game::developer_staff::bug_creation;
namespace staff=mt2mp::game::developer_staff;
namespace action=mt2mp::action_session;
namespace fixture {
std::map<uintptr_t,unsigned char> memory;
std::map<std::string,uintptr_t> parents;
std::map<uintptr_t,std::vector<uintptr_t>> collections;
std::map<uintptr_t,create::Bug> live;
action::Context context;action::Binding binding;staff::Capture capture;
std::vector<staff::job_census::Job> jobs;
bool active=true,scalar_failure=false,ledger_drift=false;
int effects=0;uintptr_t next_bug=0x50000;
std::vector<create::Value> creation_inputs;
constexpr uintptr_t parent=0x20000,collection=0x30000,lounge=0x40000,data=0x60000;
const std::string snapshot="11111111-1111-4111-8111-111111111111";
const std::string action_id="22222222-2222-4222-8222-222222222222";
const std::string key="action:"+action_id;
template<class T>void put(uintptr_t ptr,const T& value){const auto* bytes=reinterpret_cast<const unsigned char*>(&value);for(size_t n=0;n<sizeof(T);++n)memory[ptr+n]=bytes[n];}
void sync(){auto& r=bugs::runtime();int32_t count=static_cast<int32_t>(live.size());put(r.tracker+0x18,data);put(r.tracker+0x20,count);put(r.tracker+0x24,count);
 size_t index=0;for(const auto& [ptr,bug]:live){put(data+8*index++,ptr);put(ptr,uintptr_t(r.image+0x1742200));put(ptr+0x50,bug.anchor);put(ptr+0x48,bug.job);}}
void add(uintptr_t pointer,const bugs::Value& v){create::Bug b;b.pointer=pointer;b.anchor=parent;b.value={v.type,v.created,v.discovered,v.toons,v.variant,v.reproducibility,v.known,v.manual,v.debugged};live.emplace(pointer,b);collections[collection].push_back(pointer);sync();}
bugs::Row row(bugs::BirthKind kind=bugs::BirthKind::Action,uint64_t occurrence=1){return {bugs::ParentKind::Building,key,{kind,kind==bugs::BirthKind::Snapshot?std::string(64,'a'):(kind==bugs::BirthKind::Action?action_id:snapshot),kind==bugs::BirthKind::Publication?5u:0u,occurrence},{1,1,-1,0,2,.1f,false,false,false}};}
void reset(){memory.clear();parents.clear();collections.clear();live.clear();jobs.clear();creation_inputs.clear();effects=0;scalar_failure=ledger_drift=false;active=true;next_bug=0x50000;
 auto& r=bugs::runtime();r.image=0x100000000;r.thread=GetCurrentThreadId();r.ready=true;r.locked=false;r.cause.clear();r.snapshot_armed=false;r.snapshot_sealed=true;r.snapshot_host=false;r.snapshot_uuid=snapshot;r.snapshot_sha=std::string(64,'a');r.lifetimes.clear();r.saved.clear();r.jobs.clear();r.saved_jobs.clear();r.frame_work.reset();r.frame_bugs={};r.frame_publication=0;r.job_occurrence=r.job_publication=r.job_action_occurrence=0;r.action.clear();r.publication=0;r.random_occurrence=r.specific_occurrence=r.record_occurrence=0;r.confirmed_actions={action_id};
 context={};context.snapshot_uuid=snapshot;context.role=action::Role::Client;binding={};binding.native_epoch=7;binding.native_generation=9;r.context=context;r.binding=binding;r.lounge=lounge;r.tracker=lounge+0x100;
 capture={};capture.image=r.image;capture.thread=r.thread;capture.local.lounge=lounge;capture.pin.origin.world_epoch=7;capture.pin.origin.generation=9;capture.pin.time_scale=0;capture.pin.pause_scale=0;
 parents[key]=parent;collections[collection]={};put(parent,uintptr_t(r.image+0x1710000));put(parent+0x18,uint64_t(42));put(parent+0xbd8,collection);put(r.image+0x13df2e0,r.tracker);put(r.image+0x13df630,uintptr_t(0x70000));sync();
}
int checks=0;
void require(bool value,const char* reason){++checks;if(!value)throw std::runtime_error(reason);}
}
namespace placement_ui_probe {bool ProcessMemory::copy(uintptr_t ptr,void* output,size_t count,std::string& error){auto* bytes=static_cast<unsigned char*>(output);for(size_t n=0;n<count;++n){auto it=fixture::memory.find(ptr+n);if(it==fixture::memory.end()){error="fixture unmapped native field";return false;}bytes[n]=it->second;}return true;}}
namespace mt2mp {Logger& Logger::instance(){static Logger logger;return logger;}void Logger::log(LogCategory,LogLevel,const char*,int,const std::string& message){std::cerr<<message<<'\n';}}
namespace mt2mp::game::typed_action_runtime {bool bug_context(action::Context& c,action::Binding& b,uint64_t& revision,bool& active,std::string&){c=fixture::context;b=fixture::binding;revision=5;active=fixture::active;return true;}}
namespace mt2mp::game::remove_diagnostics {
bool resolve_record_target(const std::string& snapshot,const std::string& key,LocalTarget& out,std::string& error){auto it=fixture::parents.find(key);if(snapshot!=fixture::snapshot||it==fixture::parents.end()){error="fixture parent missing";return false;}out={};out.object=it->second;out.uid=42;out.snapshot_id=snapshot;out.record_key=key;out.normal_building=true;out.world_epoch=7;out.world_generation=9;return true;}
bool resolve_local_target(uintptr_t,uintptr_t object,LocalTarget& out,std::string& error){for(const auto& [key,ptr]:fixture::parents)if(ptr==object)return resolve_record_target(fixture::snapshot,key,out,error);error="fixture parent receiver missing";return false;}
}
namespace mt2mp::game::npc_probe::action_runtime {
bool local_key(uintptr_t object,uint64_t,std::string& key,std::string& e){for(const auto& [k,p]:fixture::parents)if(p==object){key=k;return true;}e="fixture NPC missing";return false;}
bool bug_parent(const action::Context&,const action::Binding&,const std::string& key,uintptr_t& ptr,std::string& e){auto it=fixture::parents.find(key);if(it==fixture::parents.end()){e="fixture NPC missing";return false;}ptr=it->second;return true;}
}
namespace mt2mp::game::developer_staff {
bool read_process(Capture& out,std::string&){out=fixture::capture;if(fixture::ledger_drift&&fixture::effects)out.pin.pause_scale=1;return true;}
bool same_origin(const Origin& a,const Origin& b){return a.world_epoch==b.world_epoch&&a.generation==b.generation;}
namespace job_census {bool read(const Read&,const Capture&,Census& out,std::string&){out={};out.jobs=fixture::jobs;return true;}}
namespace bug_creation {
bool observe(const Read&,const Lease& lease,Snapshot& out,std::string&){out={};out.collection=fixture::collections[lease.collection];for(const auto& [ptr,bug]:fixture::live){out.all.push_back(ptr);out.bugs.push_back(bug);if(bug.value.known)out.known.push_back(ptr);}return true;}
bool NativeServices::authorize(const Lease& lease,std::string& e){return authorize_(lease,e);}
bool NativeServices::read(const Lease& lease,Snapshot& out,std::string& e){return observe(read_,lease,out,e);}
bool NativeServices::add_specific(const Lease&,const Value&,const Anchor&,uintptr_t&,std::string& e){e="unexpected direct insertion fixture";return false;}
bool NativeServices::scalars(const Lease&,uintptr_t bug,const Value& value,std::string& e){++fixture::effects;fixture::live.at(bug).value=value;if(fixture::scalar_failure){e="retained scalar native failure";return false;}return true;}
bool NativeServices::anchor(const Lease&,uintptr_t,std::string&){return true;}
bool NativeServices::ensure_collection(const Lease& owner,CollectionReceipt& out){out={};out.requested=owner;out.admitted=out.confirmed=true;out.collection=owner.collection;return true;}
}}
namespace mt2mp::game::bug_authority {
bool native_code(std::string&){return true;}bool install_native(std::string&){runtime().ready=true;return true;}
void anchor_noop(uintptr_t,uintptr_t){}create::SetAnchor anchor_original(){return anchor_noop;}
bool create_replica(create::NativeServices&,const create::Lease& owner,const create::Value& value,create::Receipt& receipt){++fixture::effects;receipt={};receipt.admitted=receipt.attempted=receipt.confirmed=true;receipt.bug=fixture::next_bug;fixture::next_bug+=0x100;
    fixture::creation_inputs.push_back(value);create::Bug bug;bug.pointer=receipt.bug;bug.anchor=owner.parent;bug.value=value;fixture::live.emplace(bug.pointer,bug);fixture::collections[owner.collection].push_back(bug.pointer);fixture::sync();return true;}
bool native_remove(uintptr_t collection,uintptr_t bug,bool& attempted,std::string& e){attempted=true;++fixture::effects;fixture::live.erase(bug);auto& rows=fixture::collections[collection];rows.erase(std::remove(rows.begin(),rows.end(),bug),rows.end());fixture::sync();return witness_removal(runtime().tracker,bug,e);}
}
void run(){using fixture::require;std::string error;bool attempted=false,unknown=false;
 fixture::reset();bugs::State target{{fixture::row()}};
 require(bugs::preflight_frame(fixture::context,fixture::binding,5,target,error),"fresh action birth preflight");
 require(fixture::effects==0,"preflight has no native effects");
 require(bugs::adopt_frame(fixture::context,fixture::binding,5,target,attempted,unknown,error),"selected birth adopts");
 require(attempted&&!unknown&&fixture::effects==1&&fixture::live.size()==1,"one exact selected insertion");
 require(bugs::adopt_frame(fixture::context,fixture::binding,5,target,attempted,unknown,error),"same state idempotent");
 require(!attempted&&!unknown&&fixture::effects==1,"same state no replay");
 target.rows[0].value.reproducibility=.7f;target.rows[0].value.variant=5;target.rows[0].value.known=true;
 const auto pointer=fixture::live.begin()->first;
 require(bugs::adopt_frame(fixture::context,fixture::binding,6,target,attempted,unknown,error),"in-place authoritative scalars");
 require(fixture::live.begin()->first==pointer&&fixture::live.at(pointer).value.known,"scalar update preserves actual receiver");
 target={};require(bugs::adopt_frame(fixture::context,fixture::binding,7,target,attempted,unknown,error),"selected omitted retirement");
 require(fixture::live.empty()&&!bugs::runtime().lifetimes.pointer(pointer),"retired receiver loses identity");
 target.rows={fixture::row()};require(!bugs::adopt_frame(fixture::context,fixture::binding,8,target,attempted,unknown,error),"retired birth cannot replay");
 require(!unknown&&!attempted&&fixture::live.empty(),"retired identity rejected before native allocation");
 fixture::reset();target.rows={fixture::row()};bugs::runtime().confirmed_actions.clear();require(!bugs::preflight_frame(fixture::context,fixture::binding,5,target,error)&&fixture::effects==0,"unexecuted action birth rejected before effects");
 fixture::reset();target.rows={fixture::row(bugs::BirthKind::Publication)};target.rows[0].birth.sequence=6;require(!bugs::preflight_frame(fixture::context,fixture::binding,5,target,error),"future publication rejected");
 fixture::reset();target.rows={fixture::row(bugs::BirthKind::Snapshot)};require(!bugs::preflight_frame(fixture::context,fixture::binding,5,target,error),"missing actual saved child rejected");
 fixture::add(pointer,target.rows[0].value);bugs::runtime().saved={{fixture::collection,{pointer}}};target.rows[0].birth.occurrence=0;
 require(bugs::preflight_frame(fixture::context,fixture::binding,5,target,error),"actual serialized child seeds snapshot identity");
 target.rows[0].birth.source=std::string(64,'b');require(!bugs::preflight_frame(fixture::context,fixture::binding,5,target,error),"foreign snapshot child digest rejected");
 fixture::reset();target.rows={fixture::row()};require(bugs::adopt_frame(fixture::context,fixture::binding,5,target,attempted,unknown,error),"failure fixture birth");
 fixture::scalar_failure=true;target.rows[0].value.toons=2;require(!bugs::adopt_frame(fixture::context,fixture::binding,6,target,attempted,unknown,error),"native scalar failure surfaces");
 require(attempted&&unknown&&error=="retained scalar native failure","native first cause retained");
 const int effects=fixture::effects;require(!bugs::adopt_frame(fixture::context,fixture::binding,6,target,attempted,unknown,error)&&fixture::effects==effects,"locked unknown cannot replay");
 fixture::reset();target.rows={fixture::row()};require(bugs::adopt_frame(fixture::context,fixture::binding,5,target,attempted,unknown,error),"linked fixture birth");
 fixture::live.begin()->second.job=999;fixture::sync();target={};require(!bugs::preflight_frame(fixture::context,fixture::binding,6,target,error),"direct native Job blocks retirement");
 fixture::live.begin()->second.job=0;fixture::sync();staff::job_census::Job job;job.bug.pointer=fixture::live.begin()->first;fixture::jobs.push_back(job);
 require(!bugs::preflight_frame(fixture::context,fixture::binding,6,target,error),"queue backlink independently blocks retirement");
 fixture::reset();target.rows={fixture::row()};fixture::ledger_drift=true;require(!bugs::adopt_frame(fixture::context,fixture::binding,5,target,attempted,unknown,error)&&unknown,"post-effect Clock drift is Unknown");
 fixture::reset();target.rows={fixture::row()};fixture::capture.pin.time_scale=1;require(!bugs::preflight_frame(fixture::context,fixture::binding,5,target,error),"Client Clock must be zero");
 fixture::reset();target.rows={fixture::row()};fixture::parents.clear();require(!bugs::preflight_frame(fixture::context,fixture::binding,5,target,error),"missing authoritative parent fails before effects");
 fixture::reset();auto host=fixture::context;host.role=action::Role::Host;fixture::context=host;bugs::runtime().context=host;bugs::State observed;
 require(bugs::capture(host,fixture::binding,5,observed,error)&&observed.rows.empty(),"actual complete empty Host state");
 fixture::add(pointer,fixture::row().value);require(!bugs::capture(host,fixture::binding,5,observed,error),"unwitnessed original birth cannot be assigned by live ordinal");
 require(bugs::witness_birth(fixture::collection,pointer,1,error),"actual Host original birth witness");
 require(bugs::capture(host,fixture::binding,5,observed,error)&&observed.rows.size()==1&&observed.rows[0].birth.kind==bugs::BirthKind::Publication,"Host birth uses existing publication");
 require(observed.rows[0].birth.sequence==5&&observed.rows[0].birth.source==fixture::snapshot,"publication identity owner exact");
 fixture::reset();const auto npc_key="action:33333333-3333-4333-8333-333333333333";const uintptr_t npc=0x21000,npc_collection=0x31000;
 fixture::parents[npc_key]=npc;fixture::collections[npc_collection]={};fixture::put(npc,bugs::runtime().image+0x1742980);fixture::put(npc+0x18,uint64_t(43));fixture::put(npc+0x848,npc_collection);
 auto npc_row=fixture::row(bugs::BirthKind::Action,2);npc_row.parent_kind=bugs::ParentKind::Npc;npc_row.parent_key=npc_key;target.rows={fixture::row(),npc_row};
 require(bugs::adopt_frame(fixture::context,fixture::binding,5,target,attempted,unknown,error),"complete frame covers Building and NPC collections");
 require(fixture::collections[fixture::collection].size()==1&&fixture::collections[npc_collection].size()==1,"each birth uses actual canonical parent collection");
 target.rows.erase(target.rows.begin());require(bugs::adopt_frame(fixture::context,fixture::binding,6,target,attempted,unknown,error),"selected Building retirement preserves NPC occurrence");
 require(fixture::live.size()==1&&fixture::live.begin()->second.anchor==npc,"unrelated NPC receiver and parent retained");
 for(const int raised_type:{1,2}){
  fixture::reset();target.rows={fixture::row()};target.rows[0].value.type=0;
  require(bugs::adopt_frame(fixture::context,fixture::binding,5,target,attempted,unknown,error),"type-zero witnessed occurrence setup");
  const auto before=*bugs::runtime().lifetimes.pointer(fixture::live.begin()->first);
  const auto count=bugs::runtime().lifetimes.rows().size();const auto effect_count=fixture::effects;
  target.rows[0].value.type=raised_type;
  require(bugs::adopt_frame(fixture::context,fixture::binding,6,target,attempted,unknown,error),"original AddRandomBug existing-type promotion adopts");
  const auto* after=bugs::runtime().lifetimes.pointer(before.bug);
  require(attempted&&!unknown&&fixture::effects==effect_count+1&&fixture::live.at(before.bug).value.type==raised_type,"type promotion uses one scalar effect");
  require(after&&bugs::same_identity(after->row,before.row)&&after->parent==before.parent&&after->collection==before.collection&&bugs::runtime().lifetimes.rows().size()==count,"type promotion preserves birth parent receiver and occurrence count");
  require(bugs::adopt_frame(fixture::context,fixture::binding,6,target,attempted,unknown,error)&&!attempted&&fixture::effects==effect_count+1,"same promoted state does not replay scalar or birth");
 }
 fixture::reset();target.rows={fixture::row()};
 require(bugs::adopt_frame(fixture::context,fixture::binding,5,target,attempted,unknown,error),"Work retirement fixture birth");
 const auto linked=fixture::live.begin()->first;fixture::live.at(linked).job=0x90000;fixture::sync();
 staff::job_census::Job linked_job;linked_job.bug.pointer=linked;fixture::jobs.push_back(linked_job);
 bugs::WorkState work;target={};const auto before_retire=fixture::effects;error.clear();
 require(bugs::preflight_frame(fixture::context,fixture::binding,6,target,error,&work),"Work preflight admits linked Bug for phased cleanup");
 require(bugs::adopt_frame(fixture::context,fixture::binding,6,target,attempted,unknown,error)&&!attempted&&!unknown&&fixture::effects==before_retire&&fixture::live.contains(linked),"Bug adopt deleted before Developer detach");
 require(!bugs::finish_frame(fixture::context,fixture::binding,6,target,attempted,unknown,error)&&!attempted&&!unknown&&fixture::effects==before_retire&&fixture::live.contains(linked),"Bug finish ignored direct/queue links");
 require(error=="Bug finish still has a native Job reference","Bug finish lost original rejection cause");
 fixture::live.at(linked).job=0;fixture::sync();error.clear();
 require(!bugs::finish_frame(fixture::context,fixture::binding,6,target,attempted,unknown,error)&&!attempted&&fixture::live.contains(linked),"Bug finish ignored surviving queue reference");
 fixture::jobs.clear();error.clear();
 require(bugs::finish_frame(fixture::context,fixture::binding,6,target,attempted,unknown,error)&&attempted&&!unknown&&!fixture::live.contains(linked)&&fixture::effects==before_retire+1,"Bug finish did not retire selected detached occurrence once");
 require(bugs::finish_frame(fixture::context,fixture::binding,6,target,attempted,unknown,error)&&!attempted&&fixture::effects==before_retire+1,"confirmed finish replayed native removal");
 fixture::reset();target.rows={fixture::row()};target.rows[0].value.manual=target.rows[0].value.debugged=true;error.clear();
 require(bugs::preflight_frame(fixture::context,fixture::binding,5,target,error,&work)&&bugs::adopt_frame(fixture::context,fixture::binding,5,target,attempted,unknown,error),"new authoritative debug/manual Bug rejected");
 require(fixture::creation_inputs.size()==1&&!fixture::creation_inputs[0].manual&&!fixture::creation_inputs[0].debugged&&fixture::effects==2,"original birth was not neutral before scalar adoption");
 require(fixture::live.begin()->second.value.manual&&fixture::live.begin()->second.value.debugged,"new Bug final scalar flags differ");
 auto prepare_job=[&](uintptr_t native_job){fixture::reset();fixture::context.role=action::Role::Host;bugs::runtime().context=fixture::context;
     fixture::add(pointer,fixture::row().value);error.clear();require(bugs::witness_birth(fixture::collection,pointer,1,error),"original Bug witness setup");
     fixture::live.at(pointer).job=native_job;fixture::sync();fixture::put(native_job,bugs::runtime().image+0x17427a0);fixture::put(native_job+8,uintptr_t(0xa0000));
     fixture::put(native_job+0x18,int32_t(7));fixture::put(native_job+0x38,pointer);};
 constexpr uintptr_t native_job=0x90000;prepare_job(native_job);
 require(bugs::work_birth(pointer,native_job,error)&&bugs::runtime().jobs.rows().size()==1,"original MakeJob witness rejected");
 require(bugs::runtime().jobs.rows()[0].row.birth==bugs::Birth{bugs::BirthKind::Publication,fixture::snapshot,5,1},"MakeJob publication birth differs");
 require(!bugs::work_birth(pointer,native_job,error)&&bugs::runtime().jobs.rows().size()==1,"duplicate original Job receiver reidentified");
 prepare_job(native_job);bugs::runtime().action=fixture::action_id;
 require(bugs::work_birth(pointer,native_job,error)&&bugs::runtime().jobs.rows()[0].row.birth==bugs::Birth{bugs::BirthKind::Action,fixture::action_id,0,1},"MakeJob action birth differs");
 for(int wrong=0;wrong<5;++wrong){prepare_job(native_job);
     if(wrong==0)fixture::put(pointer+0x48,uintptr_t(native_job+1));if(wrong==1)fixture::put(native_job+0x38,uintptr_t(pointer+1));
     if(wrong==2)fixture::put(pointer+0x50,uintptr_t(fixture::parent+1));if(wrong==3)fixture::put(native_job,bugs::runtime().image+0x1742200);if(wrong==4)fixture::put(native_job+0x18,int32_t(0));
     require(!bugs::work_birth(pointer,native_job,error)&&bugs::runtime().jobs.rows().empty(),"wrong MakeJob reciprocal/parent/type admitted");}
 prepare_job(native_job);bugs::runtime().job_publication=5;bugs::runtime().job_occurrence=UINT64_MAX;
 require(!bugs::work_birth(pointer,native_job,error)&&bugs::runtime().jobs.rows().empty(),"Job publication occurrence overflow accepted");
 prepare_job(native_job);bugs::runtime().snapshot_armed=true;bugs::runtime().snapshot_host=true;
 require(bugs::work_saved(native_job,0xb0000,12,error)&&bugs::runtime().saved_jobs.size()==1,"Host snapshot save wrongly requires loaded receiver context");
 require(!bugs::work_saved(native_job,0xb0000,13,error),"same saved Job receiver duplicated");
 prepare_job(native_job);bugs::runtime().snapshot_armed=true;bugs::runtime().snapshot_host=false;
 require(!bugs::work_saved(native_job,0xb0000,12,error)&&bugs::runtime().saved_jobs.empty(),"Client snapshot accepted foreign context");
 require(bugs::work_saved(native_job,0xa0000,12,error)&&bugs::runtime().saved_jobs.size()==1,"Client actual snapshot context/link rejected");
 prepare_job(native_job);bugs::runtime().snapshot_armed=true;
 require(!bugs::work_saved(native_job,0xa0000,0,error)&&bugs::runtime().saved_jobs.empty(),"zero serialized Job link accepted");
 fixture::reset();require(bugs::work_saved(1,2,3,error)&&bugs::runtime().saved_jobs.empty(),"undistributed capture invented snapshot Job witnesses");
 std::cout<<fixture::checks<<" actual runtime boundary checks PASS\n";
}
int main(){try{run();return 0;}catch(const std::exception& error){std::cerr<<"Runtime check failed: "<<error.what()<<'\n';return 1;}}
