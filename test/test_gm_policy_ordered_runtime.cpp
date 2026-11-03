#include "typed_action/tick_authority.h"
#include "typed_action/tick_evidence.h"
#include "gm_policy_native.h"
#include "gm_policy_spatial.h"
#include <atomic>
#include <iostream>
#include <type_traits>
namespace real=mt2mp::action_session;
namespace tick=real::tick_commands;
namespace actual=mt2mp::game::gm_policy;
using json=nlohmann::json;
unsigned fixture_checks=0;
void need(bool value,const std::string& cause){++fixture_checks;if(!value)throw std::runtime_error(cause);}
namespace fixture {
bool fail(std::string& error,const std::string& cause){error=cause;return false;}
namespace a {
using namespace real;
struct Selected {Context value;bool fresh=true;bool current(Context& out,std::string& error){if(!fresh)return fail(error,"fixture Session changed");out=value;return true;}};
Selected fixture_selected;Selected& selected_session(){return fixture_selected;}
namespace tick_commands::authority {
bool active(){return tick::authority::active();}
bool matches(const Context& context,const Binding& binding,const Request& request,std::string& error){return tick::authority::matches(context,binding,request,error);}
}
}
struct Config {bool is_host=true;static Config& instance(){static Config value;return value;}};
struct Local {unsigned pin=1;std::vector<uintptr_t> gm_rows{1000,2000,3000};std::vector<uint64_t> gm_uids{1,2,3};};
struct Runtime {bool ready=true,replay=false,pending=false;unsigned thread=7,captures=0,submitted=0,rejected=0;uintptr_t image=0x10000000;};
struct Receipt {a::Binding binding;a::Request request;a::Evidence evidence;bool attempted=false,returned=false,confirmed=false,retired=false,ordered=false;std::string cause;};
namespace gp {
using actual::Intent;using actual::State;using actual::Proof;using actual::Field;using actual::Cascade;using actual::SpatialModel;using actual::Spatial;
using actual::valid;using actual::ordered;using actual::effect;using actual::expected;using actual::spatial_expected;using actual::names;using actual::offsets;using actual::integer;using actual::maximum;
namespace native {actual::native::Receipt change(uintptr_t,uintptr_t,const Intent&);}
}
struct PolicyPending {a::Context context;a::Binding binding;a::GmPolicyIntent intent;std::string id;bool ordered=false;};
struct PolicyCascadeLocal {gp::Cascade value;std::array<uintptr_t,2> managers{3000,4000};std::vector<gp::SpatialModel> models;gp::Spatial spatial;};
struct PolicyReceipt:Receipt {PolicyCascadeLocal before_cascade,after_cascade;PolicyReceipt(const a::Binding& b,const a::Request& q){binding=b;request=q;}};
struct PolicyRuntime {struct Route{void* original=nullptr;};std::array<Route,8> routes;std::optional<PolicyPending> pending;std::map<std::string,PolicyReceipt> operations;}policy;
Runtime fixture_runtime;std::atomic<Runtime*> published{&fixture_runtime};
const a::Binding fixture_binding{3,4};
const std::string fixture_snapshot="11111111-1111-4111-8111-111111111111",fixture_id="22222222-2222-4222-8222-222222222222";
gp::State fixture_policies;PolicyCascadeLocal fixture_cascade,fixture_after;
real::Ledger fixture_money;json fixture_controller;
bool fixture_go=false,fixture_binding_fresh=true,fixture_routes_fresh=true,fixture_owners_fresh=true;
unsigned fixture_native_calls=0,fixture_submissions=0;int fixture_fault=0;uintptr_t fixture_queue=5000;
std::map<std::pair<uintptr_t,size_t>,uintptr_t> fixture_fields;std::map<uintptr_t,std::string> fixture_text;
unsigned GetCurrentThreadId(){return 7;}
namespace typed_action_runtime {
json controller_status(){return fixture_controller;}
bool original_go_admission(bool& inside,std::string&){inside=fixture_go;return true;}
bool read_binding(const std::string& snapshot,a::Binding& out,std::string& error){if(!fixture_binding_fresh||snapshot!=fixture_snapshot)return fail(error,"fixture Binding changed");out=fixture_binding;return true;}
bool network_binding(const std::string& snapshot,a::Binding& out,std::string& error){if(fixture_go)return fail(error,"fixture Network binding inside original Go");return read_binding(snapshot,out,error);}
bool submit_gm_policy(const std::string&,const a::GmPolicyIntent&,std::string&){++fixture_submissions;return true;}
}
namespace live {struct NativePort {NativePort(const a::Context&,const a::Binding&,void*){}bool current_owners(unsigned,std::string& error){return fixture_owners_fresh||fail(error,"fixture Native owner changed");}};}
bool policy_route_code(Runtime&,std::string& error){return fixture_routes_fresh||fail(error,"fixture Native route changed");}
bool rules_pending(){return false;}
bool intervention_pending(){return false;}
namespace control_input::runtime {bool pending(){return false;}}
bool fixture_connected=true,fixture_district_handled=false,fixture_rules_handled=false,fixture_boolean_last=false;
unsigned fixture_boolean_calls=0,fixture_district_calls=0,fixture_rules_calls=0;
unsigned fixture_integer_calls=0;int32_t fixture_integer_last=0;
void fixture_integer_original(void*,int32_t desired){++fixture_integer_calls;fixture_integer_last=desired;}
void fixture_boolean_original(void*,bool desired){++fixture_boolean_calls;fixture_boolean_last=desired;}
namespace net::session_wire {bool connected(){return fixture_connected;}}
namespace ui_move_runtime::district_edit {bool checkbox(uintptr_t,bool,void* original,bool& handled,std::string&){need(original==reinterpret_cast<void*>(&fixture_boolean_original),"shared district route lost original checkbox");++fixture_district_calls;handled=fixture_district_handled;return true;}}
bool rules_checkbox(Runtime&,uintptr_t,bool,bool& handled,std::string&){++fixture_rules_calls;handled=fixture_rules_handled;return true;}
std::string fixture_digest(const json& value){const auto bytes=value.dump();return mt2mp::sha256(std::vector<unsigned char>(bytes.begin(),bytes.end()));}
bool policy_proof(Runtime&,const a::Context&,const a::Binding&,Local& local,a::GmPolicyProof& out,a::Ledger& money,uintptr_t& queue,std::string&,PolicyCascadeLocal* cascade=nullptr,const PolicyCascadeLocal* =nullptr){
    local={};queue=fixture_queue;out={};out.state=fixture_policies;out.gm_sha256=fixture_digest(mt2mp::game::gm_ticket::encode(fixture_cascade.value.gm));out.cascade_sha256=fixture_digest(actual::encode(fixture_cascade.value));out.protected_sha256=std::string(64,fixture_fault==3&&fixture_native_calls?'c':'a');out.tutorial_sha256=std::string(64,'b');money=fixture_money;if(cascade)*cascade=fixture_cascade;return true;
}
bool policy_values(Runtime&,gp::State& out,uintptr_t& queue,std::string&){out=fixture_policies;queue=fixture_queue;return true;}
std::string uuid(std::string&){return fixture_id;}
void reject(Runtime& runtime,const std::string&){++runtime.rejected;}
bool ap(Runtime& runtime,uintptr_t object,uintptr_t expected,std::string& error){return (fixture_fields[{object,0}]==runtime.image+expected)||fail(error,"fixture UI class changed");}
template<class T>bool at(Runtime&,uintptr_t object,size_t offset,T& out,std::string& error){const auto found=fixture_fields.find({object,offset});if(found==fixture_fields.end())return fail(error,"fixture Native field absent");out=T(found->second);return true;}
bool text(Runtime&,uintptr_t object,std::string& out,size_t,std::string& error){if(!fixture_text.contains(object))return fail(error,"fixture Native text absent");out=fixture_text.at(object);return true;}
#include "policy_binding_actual.inc"
#include "policy_capture_actual.inc"
#include "policy_queue_actual.inc"
#include "policy_boolean_actual.inc"
#include "policy_integer_actual.inc"
#include "policy_runtime_actual.inc"
namespace gp::native {
actual::native::Receipt change(uintptr_t,uintptr_t,const Intent& intent){++fixture_native_calls;fixture_policies.policies[intent.policy][size_t(intent.field)]=intent.value;fixture_cascade=fixture_after;
    if(fixture_fault==2)fixture_policies.policies[1][2]^=1;
    if(fixture_fault==4&&!fixture_cascade.value.gm.tickets.empty())fixture_cascade.value.gm.tickets[0].type=1;
    if(fixture_fault==5)std::reverse(fixture_cascade.value.managers[1].begin(),fixture_cascade.value.managers[1].end());
    if(fixture_fault==6)fixture_cascade.spatial.models.members.push_back(10);
    if(fixture_fault==7)++fixture_money.cash;
    if(fixture_fault==8)++fixture_queue;
    return {true,fixture_fault!=1,fixture_fault==1?1u:0u};
}
}
struct Session:real::Session {bool current(real::Context& out,std::string& error)override{return a::fixture_selected.current(out,error);}};
struct Engine:real::Engine {
    bool fresh=true;bool bind(const std::string&,real::Binding& out,std::string&)override{out=fixture_binding;return true;}
    bool current(const real::Binding& binding,const std::string& snapshot,std::string& error)override{return (fresh&&binding==fixture_binding&&snapshot==fixture_snapshot)||fail(error,"fixture Engine lifetime changed");}
    bool ledger(const real::Binding&,real::Ledger& out,std::string&)override{out=fixture_money;return true;}
    real::Outcome execute_remove(const real::Binding&,const real::Request&,std::string&)override{return real::Outcome::RejectedNoEffect;}
    real::Outcome apply_remove(const real::Binding&,const real::Request&,const real::Evidence&,std::string&)override{return real::Outcome::RejectedNoEffect;}
    bool observe_remove(const real::Binding&,const real::Request&,real::Evidence&,std::string&)override{return false;}
    bool retire(const real::Binding&,const real::Request&,std::string&)override{return true;}
};
void reset(bool host=true,bool empty=false){
    fixture_runtime={};policy={};fixture_policies={};fixture_cascade={};fixture_cascade.managers={3000,4000};fixture_fault=0;fixture_queue=5000;
    fixture_go=false;fixture_binding_fresh=fixture_routes_fresh=fixture_owners_fresh=true;fixture_native_calls=fixture_submissions=0;
    fixture_fields.clear();fixture_text.clear();fixture_money={};fixture_money.record_present=true;fixture_money.cash=fixture_money.record_cash=1000;
    a::fixture_selected.fresh=true;a::fixture_selected.value={77,host?1u:2u,1,3,host?real::Role::Host:real::Role::Client,fixture_snapshot};Config::instance().is_host=host;
    fixture_controller={{"started",true},{"ordered",true},{"locked",false},{"pending",false},{"action_uuid",fixture_id}};
    for(int32_t slot=0;slot<3;++slot){mt2mp::game::gm_ticket::MasterValue master;master.slot=slot;master.orientation={0,0,0,1};master.scale={1,1,1};master.employed=true;master.active=true;master.on_map=!empty;master.state=empty?0:1;master.ticket=empty?-1:slot;fixture_cascade.value.gm.masters.push_back(master);fixture_cascade.value.models.push_back(!empty);}
    if(!empty){for(int32_t index=0;index<3;++index){mt2mp::game::gm_ticket::TicketValue ticket;ticket.type=index==0?2:0;ticket.master=index;ticket.valid=true;fixture_cascade.value.gm.tickets.push_back(ticket);}fixture_cascade.value.managers[1]={0,1,2};}
    else fixture_cascade.value.managers[0]={0,1,2};
    auto& spatial=fixture_cascade.spatial;spatial.tree=6000;spatial.node_data=7000;spatial.bucket_data=8000;spatial.node_count=1;spatial.buckets=2;spatial.shift=31;
    spatial.models={9000,empty?0:3,4,empty?std::vector<uintptr_t>{}:std::vector<uintptr_t>{10,20,30}};spatial.pending={10000,empty?0:1,4,empty?std::vector<uintptr_t>{}:std::vector<uintptr_t>{10}};
    for(uintptr_t object:{10u,20u,30u}){fixture_cascade.models.push_back({empty?0:object,empty?0:object*100+0x30,empty?-1:0,0});if(!empty)spatial.chains[(uint32_t(object)*uint32_t(0x9e3779ff))>>31].push_back(object);}
    if(!empty){actual::SpatialNode node;node.total=3;node.models={11000,3,4,{10,20,30}};spatial.nodes.emplace(0,node);}
    fixture_after=fixture_cascade;
    if(!empty){auto& target=fixture_after.value;target.gm.tickets.erase(target.gm.tickets.begin());target.gm.masters[0].ticket=-1;target.gm.masters[0].state=0;target.gm.masters[0].on_map=false;target.gm.masters[1].ticket=0;target.gm.masters[2].ticket=1;target.models[0]=false;target.managers[0]={0};target.managers[1]={1,2};fixture_after.models[0]={};
        auto& after=fixture_after.spatial;after.models.members={30,20};after.models.count=2;after.pending.members.clear();after.pending.count=0;auto& node=after.nodes.at(0);node.models.members={30,20};node.models.count=2;node.total=2;
        auto& chain=after.chains.at((uint32_t(10)*uint32_t(0x9e3779ff))>>31);chain.erase(std::find(chain.begin(),chain.end(),10));}
}
real::Request request(){real::Request value;value.action=real::Action::GmPolicy;value.peer=2;value.sequence=1;value.action_uuid=fixture_id;value.snapshot_uuid=fixture_snapshot;value.canonical_record_key="management:gm-policy";value.gm_policy={0,actual::Field::CheckLog,0,1};return value;}
std::string consumed(bool host_role,int defect=0,bool empty=false,bool enabled_command=false,bool checklog_no_removal=false,std::optional<actual::Intent> offered={}){
    reset(host_role,empty);fixture_fault=defect;fixture_controller["pending"]=true;
    auto desired=request();if(enabled_command){fixture_policies.policies[0][0]=1;desired.gm_policy={0,actual::Field::Enabled,1,0};}
    if(checklog_no_removal){fixture_policies.policies[0][0]=1;fixture_after=fixture_cascade;}
    if(offered){desired.gm_policy=*offered;fixture_policies.policies[0][0]=fixture_policies.policies[1][0]=1;fixture_policies.policies[offered->policy][size_t(offered->field)]=offered->before;
        if(!empty){fixture_cascade.value.gm.tickets[0].type=offered->policy?3:2;if(offered->field!=actual::Field::Enabled||offered->value)fixture_after=fixture_cascade;}
    }
    tick::Config hc;hc.context=a::fixture_selected.value;hc.context.local_peer=1;hc.context.role=real::Role::Host;tick::Config cc;cc.context=a::fixture_selected.value;
    auto validate=[](const tick::Input&,std::string&){return true;};tick::Queue host(hc,validate),client(cc,validate);tick::Input input{77,fixture_snapshot,desired};tick::Assigned assigned;tick::Batch batch;tick::Receipt receipt;std::string error,digest;
    need(host.offer(2,input,assigned,error)==tick::Admission::Accepted&&host.seal(batch,error),error);if(!host_role)need(client.admit(1,batch,error)==tick::Admission::Accepted,error);
    auto& queue=host_role?host:client;Session session;Engine engine;
    const bool ok=queue.consume(1,[](std::string&){return true;},[&](const tick::Assigned& command,std::string& cause){return tick::authority::run(queue,session,engine,fixture_binding,command,[&](std::string& why){
        const auto& q=std::get<real::Request>(command.input.command);auto wrong=q;++wrong.sequence;need(policy_perform(fixture_binding,wrong,nullptr,why)==a::Outcome::RejectedNoEffect&&!fixture_native_calls,"wrong consumed command reached policy original");
        auto changed=fixture_binding;++changed.native_generation;need(policy_perform(changed,q,nullptr,why)==a::Outcome::RejectedNoEffect&&!fixture_native_calls,"wrong policy Binding reached original");
        Config::instance().is_host=!host_role;need(policy_perform(fixture_binding,q,nullptr,why)==a::Outcome::RejectedNoEffect&&!fixture_native_calls,"foreign actual policy role reached original");Config::instance().is_host=host_role;
        fixture_owners_fresh=false;need(policy_perform(fixture_binding,q,nullptr,why)==a::Outcome::RejectedNoEffect&&!fixture_native_calls,"stale policy native owner reached original");fixture_owners_fresh=true;
        a::Evidence adopted;need(policy_perform(fixture_binding,q,&adopted,why)==a::Outcome::RejectedNoEffect&&!fixture_native_calls,"policy adopted Host result");
        auto outcome=policy_perform(fixture_binding,q,nullptr,why);need(fixture_native_calls==1,"policy original count differs");
        if(defect){need(outcome==a::Outcome::Unknown,"partial policy cascade confirmed");need(policy_perform(fixture_binding,q,nullptr,why)==a::Outcome::Unknown&&fixture_native_calls==1,"unknown policy replayed");return outcome;}
        need(outcome==a::Outcome::Confirmed,why);a::Evidence evidence;engine.fresh=false;need(!policy_observe(fixture_binding,q,evidence,why),"stale policy engine observed");engine.fresh=true;
        need(policy_observe(fixture_binding,q,evidence,why)&&policy_retire(fixture_binding,q,why)&&policy_observe(fixture_binding,q,evidence,why),why);
        tick::ExecutionReceipt executed;executed.command=command;executed.outcome=outcome;executed.evidence=std::make_shared<a::Evidence>(evidence);need(tick::canonical_evidence(a::fixture_selected.value,command,executed,digest,why),why);
        auto absent=evidence;absent.gm_policy_before->cascade_sha256.clear();absent.gm_policy_before->protected_sha256.clear();absent.gm_policy_before->tutorial_sha256.clear();executed.evidence=std::make_shared<a::Evidence>(absent);std::string unused;need(!tick::canonical_evidence(a::fixture_selected.value,command,executed,unused,why),"missing ordered policy witness became canonical");need(queue.bind_evidence(command,digest,why),why);return outcome;
    },cause);},[](uint64_t,std::string&){return real::Outcome::Confirmed;},receipt,error);
    need(ok==!defect,"policy consumed queue outcome differs");need(!tick::authority::active(),"policy consumed authority escaped");a::Evidence outside;need(!policy_observe(fixture_binding,desired,outside,error),"policy receipt observed outside consumed scope");return digest;
}
void run(){
    reset();std::string error;auto q=request();fixture_go=true;need(policy_queue(fixture_runtime,q.gm_policy,error)&&policy.pending&&policy.pending->ordered&&!fixture_native_calls,"policy before-write original-Go capture failed");fixture_go=false;policy_tick(fixture_runtime);need(fixture_submissions==1&&!policy.pending&&!fixture_native_calls,"policy held pump did not submit exactly once");policy_tick(fixture_runtime);need(fixture_submissions==1,"policy submitted twice");
    for(int drift=0;drift<3;++drift){reset();q=request();need(policy_queue(fixture_runtime,q.gm_policy,error),error);if(!drift)++a::fixture_selected.value.session;if(drift==1)fixture_binding_fresh=false;if(drift==2)fixture_controller["ordered"]=false;policy_tick(fixture_runtime);need(!fixture_submissions&&fixture_runtime.rejected==1,"stale policy capture submitted");}
    reset();q=request();q.gm_policy.policy=2;need(!policy_queue(fixture_runtime,q.gm_policy,error),"foreign policy admitted");q=request();q.gm_policy.field=actual::Field(7);need(!policy_queue(fixture_runtime,q.gm_policy,error),"foreign policy field admitted");
    reset();q=request();fixture_controller["pending"]=true;need(policy_perform(fixture_binding,q,nullptr,error)==a::Outcome::RejectedNoEffect&&!fixture_native_calls,"policy executed outside consumed authority");
    const auto host=consumed(true),client=consumed(false);need(!host.empty()&&host==client,"policy canonical Host/Client cascade differs");need(consumed(true,0,true)==consumed(false,0,true),"ordinary empty queue policy unavailable or noncanonical");
    need(consumed(true,0,false,true)==consumed(false,0,false,true),"original Chat disable/removal canonical effects differ");
    need(consumed(true,0,false,false,true)==consumed(false,0,false,false,true),"CheckLog on enabled chat changed queue or canonical proof");
    need(consumed(true,0,true,true)==consumed(false,0,true,true),"ordinary empty queue Chat Enabled unavailable");
    for(uint8_t p=0;p<2;++p)for(size_t f=0;f<actual::names.size();++f){const auto field=actual::Field(f);const actual::Intent offered{p,field,field==actual::Field::Enabled?1:0,field==actual::Field::Enabled?0:actual::maximum(field)};
        need(real::ordered_gm_policy(offered)&&actual::codec::intent(actual::encode(offered))==offered,"offered policy intent/codec lost field");
        const auto left=consumed(true,0,false,false,false,offered);need(!left.empty()&&left==consumed(false,0,false,false,false,offered),"offered Chat/Cheat field canonical cascade differs");
        need(consumed(true,0,true,false,false,offered)==consumed(false,0,true,false,false,offered),"empty queue policy field unavailable");
        auto invalid=offered;invalid.value=actual::maximum(field)+1;need(!real::ordered_gm_policy(invalid),"out-of-range policy field admitted");invalid=offered;invalid.value=invalid.before;need(!real::ordered_gm_policy(invalid),"unchanged policy field admitted");
    }
    for(int defect=1;defect<=8;++defect){consumed(true,defect);consumed(false,defect);}
    reset();fixture_fields={{{10000,0},fixture_runtime.image+0x16d98c0},{{10000,0x3c8},11000},{{11000,0},fixture_runtime.image+0x1749610},{{11000,8},fixture_queue+0x48},{{11000,0x38},fixture_queue+0x68},{{fixture_runtime.image,0x13df360},fixture_queue}};fixture_text={{11000+0x18,"checkLog"}};
    actual::Intent captured;bool ours=false;need(policy_capture(fixture_runtime,10000,true,captured,error,ours)&&ours&&captured.policy==0&&captured.field==actual::Field::CheckLog&&captured.before==0,"actual bound chat CheckLog capture differs");fixture_fields[{11000,0x38}]++;need(!policy_capture(fixture_runtime,10000,true,captured,error,ours),"foreign scalar address captured");
    fixture_fields[{11000,0x38}]=fixture_queue+0x60;fixture_text[11000+0x18]="enabled";need(policy_capture(fixture_runtime,10000,true,captured,error,ours)&&ours&&captured.field==actual::Field::Enabled,"actual bound Chat Enabled capture differs");
}
void integer_routes(){
    for(uint8_t p=0;p<2;++p)for(const auto field:{actual::Field::WatchSeconds,actual::Field::WarnsBeforeBan})for(size_t index=3;index<=6;++index){
        reset();fixture_go=true;fixture_integer_calls=0;
        for(auto& route:policy.routes)route.original=reinterpret_cast<void*>(&fixture_integer_original);
        fixture_fields={{{10000,0},fixture_runtime.image+0x16e7070},{{10000,0x420},11000},{{10000,0x320},0},{{10000,0x324},uintptr_t(actual::maximum(field))},{{10000,0x32c},1},{{10000,0x330},1},{{10000,0x428},0},{{11000,0},fixture_runtime.image+0x17496b0},{{11000,8},fixture_queue+(p?0x70:0x48)},{{11000,0x38},fixture_queue+(p?0x70:0x48)+actual::offsets[size_t(field)]},{{fixture_runtime.image,0x13df360},fixture_queue}};fixture_text={{11000+0x18,actual::names[size_t(field)]}};
        policy_integer(index,reinterpret_cast<void*>(10000),actual::maximum(field));need(policy.pending&&policy.pending->intent==actual::Intent{p,field,0,actual::maximum(field)}&&!fixture_native_calls&&!fixture_integer_calls,"offered numeric entry lost before-store capture");
        policy.pending.reset();fixture_fields[{10000,0x324}]++;policy_integer(index,reinterpret_cast<void*>(10000),1);need(!policy.pending&&fixture_runtime.rejected==1,"wrong original policy spinner range captured");
        fixture_fields[{11000,8}]=9999;policy_integer(index,reinterpret_cast<void*>(10000),99);need(fixture_integer_calls==1&&fixture_integer_last==99&&!policy.pending,"unrelated original spinner did not forward exactly once");
        fixture_runtime.replay=true;policy_integer(index,reinterpret_cast<void*>(10000),7);need(fixture_integer_calls==2&&fixture_integer_last==7&&!policy.pending,"spinner replay did not retain original route");
    }
}
void boolean_routes(){reset();for(auto& route:policy.routes)route.original=reinterpret_cast<void*>(&fixture_boolean_original);
    fixture_connected=true;fixture_district_handled=fixture_rules_handled=false;fixture_boolean_calls=fixture_district_calls=fixture_rules_calls=0;
    fixture_fields={{{10000,0},fixture_runtime.image+0x16d98c0},{{10000,0x3c8},11000},{{11000,0},fixture_runtime.image+0x1749610},{{11000,8},fixture_queue+0x48},{{11000,0x38},fixture_queue+0x60},{{fixture_runtime.image,0x13df360},fixture_queue}};fixture_text={{11000+0x18,"enabled"}};
    fixture_go=true;policy_checkbox(reinterpret_cast<void*>(10000),true);need(policy.pending&&policy.pending->intent.field==actual::Field::Enabled&&fixture_policies.policies[0][0]==0&&!fixture_native_calls&&!fixture_boolean_calls,"shared original Enabled entry did not capture before store");policy.pending.reset();
    fixture_fields[{11000,0x38}]=fixture_queue+0x68;fixture_text[11000+0x18]="checkLog";policy_toggled(reinterpret_cast<void*>(10000),true);need(policy.pending&&policy.pending->intent.field==actual::Field::CheckLog&&!fixture_boolean_calls&&!fixture_native_calls,"shared original CheckLog entry lost capture");policy.pending.reset();
    fixture_fields[{11000,8}]=9999;fixture_district_handled=true;policy_checkbox(reinterpret_cast<void*>(10000),true);need(fixture_district_calls==1&&!fixture_rules_calls&&!fixture_boolean_calls&&!policy.pending,"shared district checkbox route did not retain ownership");
    fixture_district_handled=false;fixture_rules_handled=true;policy_checkbox(reinterpret_cast<void*>(10000),true);need(fixture_district_calls==2&&fixture_rules_calls==1&&!fixture_boolean_calls&&!policy.pending,"shared rules checkbox route did not retain ownership");
    fixture_rules_handled=false;policy_checkbox(reinterpret_cast<void*>(10000),true);need(fixture_boolean_calls==1&&fixture_boolean_last&&!policy.pending,"unrelated original checkbox did not forward once");
    fixture_runtime.replay=true;policy_checkbox(reinterpret_cast<void*>(10000),false);need(fixture_boolean_calls==2&&!fixture_boolean_last&&!policy.pending,"original replay recaptured checkbox");fixture_runtime.replay=false;
    fixture_connected=false;policy_checkbox(reinterpret_cast<void*>(10000),true);need(fixture_boolean_calls==3&&fixture_boolean_last&&!policy.pending,"inactive session checkbox did not forward");fixture_connected=true;
    for(uint8_t p=0;p<2;++p)for(size_t f=0;f<actual::names.size();++f){const auto field=actual::Field(f);if(actual::integer(field))continue;reset();fixture_go=true;fixture_boolean_calls=0;for(auto& route:policy.routes)route.original=reinterpret_cast<void*>(&fixture_boolean_original);
        fixture_fields={{{10000,0},fixture_runtime.image+0x16d98c0},{{10000,0x3c8},11000},{{11000,0},fixture_runtime.image+0x1749610},{{11000,8},fixture_queue+(p?0x70:0x48)},{{11000,0x38},fixture_queue+(p?0x70:0x48)+actual::offsets[f]},{{fixture_runtime.image,0x13df360},fixture_queue}};fixture_text={{11000+0x18,actual::names[f]}};
        policy_checkbox(reinterpret_cast<void*>(10000),true);need(policy.pending&&policy.pending->intent==actual::Intent{p,field,0,1}&&!fixture_boolean_calls&&!fixture_native_calls,"offered Chat/Cheat checkbox lost original before-store capture");
    }
}
}
#include "gm_policy_reader_cases.inc"
#include "gm_policy_ui_cases.inc"
int main()try{fixture::run();fixture::boolean_routes();fixture::integer_routes();reader_fixture::run();policy_ui_test::run();std::cout<<fixture_checks<<" ordered GM policy production-body/consumed Queue assertions passed\n";return 0;}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
