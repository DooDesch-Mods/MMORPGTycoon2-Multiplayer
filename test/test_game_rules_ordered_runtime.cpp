#include "typed_action/tick_authority.h"
#include "typed_action/tick_evidence.h"
#include "game_rules_native.h"
#include <atomic>
#include "rules_command_fixture.h"
#include <cstring>
#include <iostream>
#include <map>
#include <set>
#include <charconv>
#include <type_traits>
namespace real=mt2mp::action_session;
namespace tick=real::tick_commands;
namespace gr=mt2mp::game::game_rules;
using json=nlohmann::json;
unsigned checks=0;
void need(bool ok,const std::string& cause){++checks;if(!ok)throw std::runtime_error(cause);}
namespace fixture {
bool fail(std::string& e,const std::string& cause){e=cause;return false;}
namespace a {
using namespace real;
struct Selected {Context value;bool fresh=true;bool current(Context& out,std::string& e){if(!fresh)return fail(e,"Session changed");out=value;return true;}};
Selected selected;Selected& selected_session(){return selected;}
namespace tick_commands::authority {
bool active(){return tick::authority::active();}
bool matches(const Context& c,const Binding& b,const Request& q,std::string& e){return tick::authority::matches(c,b,q,e);}
}
}
struct Config {bool is_host=true;static Config& instance(){static Config out;return out;}};
struct Local {unsigned pin=1;};
struct Receipt {a::Binding binding;a::Request request;a::Evidence evidence;bool attempted=false,returned=false,confirmed=false,retired=false,ordered=false;std::string cause;};
struct Runtime {bool ready=true,replay=false;unsigned thread=7;uintptr_t image=100;bool pending=false;unsigned captures=0,submitted=0,rejected=0;};
struct RulesPending {a::Context context;a::Binding binding;a::GameRulesIntent intent;std::string id;bool ordered=false;};
using Ui=bool(*)(void*,void*);
bool fixture_forward(void*,void*);
struct RulesRuntime {struct Route{Ui original=&fixture_forward;};std::array<Route,3> routes;std::optional<RulesPending> pending;std::map<std::string,Receipt> operations;}design_rules;
Runtime fixture_runtime;std::atomic<Runtime*> published{&fixture_runtime};
a::GameRulesProof fixture_state;gr::Pin fixture_pin;real::Ledger fixture_money;
unsigned native_calls=0,submissions=0;bool go=false,binding_fresh=true,routes_fresh=true,owners_fresh=true;int fault=0;
std::map<std::pair<uintptr_t,size_t>,uintptr_t> fixture_ui_fields;
std::map<uintptr_t,std::string> fixture_ui_text;
std::array<uint32_t,14> fixture_ui_draft{};
unsigned fixture_forwards=0,fixture_button_calls=0;bool fixture_button_fault=false,fixture_parse_valid=true;
const a::Binding fixture_binding{3,4};const std::string fixture_snapshot="11111111-1111-4111-8111-111111111111",fixture_id="22222222-2222-4222-8222-222222222222";
json fixture_controller;
unsigned GetCurrentThreadId(){return 7;}
namespace typed_action_runtime {
json controller_status(){return fixture_controller;}
bool original_go_admission(bool& inside,std::string&){inside=go;return true;}
bool read_binding(const std::string& source,a::Binding& out,std::string& e){if(!binding_fresh||source!=fixture_snapshot)return fail(e,"Binding changed");out=fixture_binding;return true;}
bool network_binding(const std::string& source,a::Binding& out,std::string& e){if(go)return fail(e,"Network fixture_binding inside original Go");return read_binding(source,out,e);}
bool submit_game_rules(const std::string&,const a::GameRulesIntent&,std::string&){++submissions;return true;}
}
namespace rules {
using gr::Pin;using gr::State;using gr::Intent;using gr::Command;using gr::valid;using gr::effect;using gr::feature_calls;using gr::feature_events;
namespace native {
uint32_t parse(uintptr_t,const std::string& text,int32_t& out,bool& parsed){auto result=std::from_chars(text.data(),text.data()+text.size(),out);parsed=fixture_parse_valid&&result.ec==std::errc{};return 0;}
uint32_t buttons(uintptr_t,void*,bool enabled){need(!enabled,"XP capture enabled buttons");++fixture_button_calls;return fixture_button_fault?1u:0u;}
gr::native::Receipt change(uintptr_t,const Pin&,const Intent& intent){++native_calls;fixture_state.state=intent.after;gr::native::Receipt out;out.attempted=true;out.returned=fault!=1;out.exception=fault==1?1u:0u;
    if(fault==2)++fixture_state.state.xp[3];if(fault==3)fixture_state.protected_sha256=std::string(64,'c');if(fault==4)++fixture_money.cash;if(fault==5)++fixture_pin.design;return out;}
}
}
namespace live {struct NativePort {NativePort(const a::Context&,const a::Binding&,void*){}bool current_owners(unsigned,std::string& e){return owners_fresh||fail(e,"Native owners changed");}};}
bool rules_routes(Runtime&,std::string& e){return routes_fresh||fail(e,"Native route changed");}
bool policy_pending(){return false;}
bool rules_proof(Runtime&,const a::Context&,const a::Binding&,Local& local,rules::Pin& owners,a::GameRulesProof& out,a::Ledger& ledger,std::string&){local.pin=unsigned(fixture_pin.design);owners=fixture_pin;out=fixture_state;ledger=fixture_money;return true;}
bool rules_values(Runtime&,rules::State& values,rules::Pin& owners,std::string&){values=fixture_state.state;owners=fixture_pin;return true;}
std::string uuid(std::string&){return fixture_id;}
void reject(Runtime& r,const std::string&){++r.rejected;}
bool fixture_forward(void*,void*){++fixture_forwards;return true;}
bool rules_active(Runtime& r){return !r.replay&&fixture_controller.value("started",false);}
bool rules_matches(Runtime&,void* record,const char* name,bool& matches,std::string& error){const auto exception=gr::native::command(native_command_fixture::image(),record,name,matches);return !exception||fail(error,"fixture original label read SEH="+std::to_string(exception));}
bool ap(Runtime& r,uintptr_t object,uintptr_t expected,std::string& e){return (fixture_ui_fields[{object,0}]==r.image+expected)||fail(e,"Native UI class differs");}
template<class T>bool at(Runtime&,uintptr_t object,size_t offset,T& out,std::string& e){
    if constexpr(std::is_same_v<T,std::array<uint32_t,14>>){if(offset!=0x350)return fail(e,"XP draft field differs");out=fixture_ui_draft;return true;}
    else{auto found=fixture_ui_fields.find({object,offset});if(found==fixture_ui_fields.end())return fail(e,"Native UI field absent");out=T(found->second);return true;}
}
bool text(Runtime&,uintptr_t object,std::string& out,size_t,std::string& e){if(!fixture_ui_text.contains(object))return fail(e,"Native UI text absent");out=fixture_ui_text.at(object);return true;}
#include "rules_capture_actual.inc"
namespace net::session_wire {bool connected(){return true;}}
#include "rules_ui_capture_actual.inc"
#include "rules_runtime_actual.inc"
struct Session:real::Session {bool current(real::Context& out,std::string& e)override{return a::selected.current(out,e);}};
struct Engine:real::Engine {
    bool fresh=true;
    bool bind(const std::string&,real::Binding& out,std::string&)override{out=fixture_binding;return true;}
    bool current(const real::Binding& b,const std::string& source,std::string& e)override{return (fresh&&b==fixture_binding&&source==fixture_snapshot)||fail(e,"Actual engine lifetime changed");}
    bool ledger(const real::Binding&,real::Ledger& out,std::string&)override{out=fixture_money;return true;}
    real::Outcome execute_remove(const real::Binding&,const real::Request&,std::string&)override{return real::Outcome::RejectedNoEffect;}
    real::Outcome apply_remove(const real::Binding&,const real::Request&,const real::Evidence&,std::string&)override{return real::Outcome::RejectedNoEffect;}
    bool observe_remove(const real::Binding&,const real::Request&,real::Evidence&,std::string&)override{return false;}
    bool retire(const real::Binding&,const real::Request&,std::string&)override{return true;}
};
real::Request request(gr::Command command=gr::Command::RespawnDelay){real::Request q;q.action=real::Action::GameRules;q.peer=2;q.sequence=1;q.action_uuid=fixture_id;q.snapshot_uuid=fixture_snapshot;q.canonical_record_key="management:game-rules";q.game_rules.command=command;q.game_rules.before=fixture_state.state;q.game_rules.after=fixture_state.state;if(command==gr::Command::RespawnDelay)q.game_rules.after.respawn_delay=!fixture_state.state.respawn_delay;else if(command==gr::Command::RespawnSeconds)q.game_rules.after.respawn_seconds=90;else if(command==gr::Command::ApplyXP)q.game_rules.after.xp[1]=20;return q;}
void reset(bool host=true){fixture_runtime={};design_rules={};fixture_state={};fixture_state.state.xp[1]=10;fixture_state.tutorial_sha256=std::string(64,'a');fixture_state.protected_sha256=std::string(64,'b');fixture_pin={};fixture_pin.design=123;
    fixture_money={};fixture_money.record_present=true;fixture_money.cash=fixture_money.record_cash=1000;native_calls=submissions=0;fault=0;go=false;binding_fresh=routes_fresh=owners_fresh=true;
    a::selected.fresh=true;a::selected.value={77,host?1u:2u,1,3,host?real::Role::Host:real::Role::Client,fixture_snapshot};Config::instance().is_host=host;
    fixture_controller={{"started",true},{"ordered",true},{"locked",false},{"pending",false},{"action_uuid",fixture_id}};
    fixture_ui_fields.clear();fixture_ui_text.clear();fixture_forwards=fixture_button_calls=0;fixture_button_fault=false;fixture_parse_valid=true;fixture_ui_draft=fixture_state.state.xp;
}
std::string consumed(bool host_role,int defect=0,gr::Command command_kind=gr::Command::RespawnDelay){
    reset(host_role);fault=defect;fixture_controller["pending"]=true;
    tick::Config hc;hc.context=a::selected.value;hc.context.local_peer=1;hc.context.role=real::Role::Host;tick::Config cc;cc.context=a::selected.value;
    auto validate=[](const tick::Input&,std::string&){return true;};tick::Queue host(hc,validate),client(cc,validate);const auto input=tick::Input{77,fixture_snapshot,request(command_kind)};tick::Assigned assigned;tick::Batch batch;tick::Receipt receipt;std::string e,digest;
    need(host.offer(2,input,assigned,e)==tick::Admission::Accepted&&host.seal(batch,e),e);if(!host_role)need(client.admit(1,batch,e)==tick::Admission::Accepted,e);
    auto& queue=host_role?host:client;Session session;Engine engine;
    auto current=[](std::string&){return true;};auto advance=[](uint64_t,std::string&){return real::Outcome::Confirmed;};
    const bool consumed_ok=queue.consume(1,current,[&](const tick::Assigned& command,std::string& cause){return tick::authority::run(queue,session,engine,fixture_binding,command,[&](std::string& why){
        const auto& q=std::get<real::Request>(command.input.command);auto wrong=q;wrong.action_uuid="wrong";need(rules_perform(fixture_binding,wrong,nullptr,why)==a::Outcome::RejectedNoEffect&&native_calls==0,"Wrong consumed command reached original");
        wrong=q;++wrong.sequence;need(rules_perform(fixture_binding,wrong,nullptr,why)==a::Outcome::RejectedNoEffect&&native_calls==0,"Wrong consumed sequence reached original");
        auto other_binding=fixture_binding;++other_binding.native_generation;need(rules_perform(other_binding,q,nullptr,why)==a::Outcome::RejectedNoEffect&&native_calls==0,"Wrong consumed native generation reached original");
        Config::instance().is_host=!host_role;need(rules_perform(fixture_binding,q,nullptr,why)==a::Outcome::RejectedNoEffect&&native_calls==0,"Foreign actual role reached original");Config::instance().is_host=host_role;
        owners_fresh=false;need(rules_perform(fixture_binding,q,nullptr,why)==a::Outcome::RejectedNoEffect&&native_calls==0,"Stale native owners reached original");owners_fresh=true;
        a::Evidence adopted;need(rules_perform(fixture_binding,q,&adopted,why)==a::Outcome::RejectedNoEffect&&native_calls==0,"Ordered rule accepted Host evidence");
        auto outcome=rules_perform(fixture_binding,q,nullptr,why);need(native_calls==1,"Consumed command did not call original once");
        if(defect){need(outcome==a::Outcome::Unknown,"Partial or unrelated original effect confirmed");need(rules_perform(fixture_binding,q,nullptr,why)==a::Outcome::Unknown&&native_calls==1,"Unknown original was replayed");return outcome;}
        need(outcome==a::Outcome::Confirmed,why);a::Evidence evidence;engine.fresh=false;need(!rules_observe(fixture_binding,q,evidence,why),"Stale actual engine lifetime observed");engine.fresh=true;
        need(rules_observe(fixture_binding,q,evidence,why)&&rules_retire(fixture_binding,q,why)&&rules_observe(fixture_binding,q,evidence,why),why);
        tick::ExecutionReceipt executed;executed.command=command;executed.outcome=outcome;executed.evidence=std::make_shared<a::Evidence>(evidence);
        need(tick::canonical_evidence(a::selected.value,command,executed,digest,why),why);
        auto absent=evidence;absent.game_rules_before->protected_sha256.clear();absent.game_rules_after->protected_sha256.clear();executed.evidence=std::make_shared<a::Evidence>(absent);std::string unused;
        need(!tick::canonical_evidence(a::selected.value,command,executed,unused,why),"Absent protected RNG/Management proof accepted");
        need(queue.bind_evidence(command,digest,why),why);return outcome;
    },cause);},advance,receipt,e);
    need(consumed_ok==!defect,"Consumed queue outcome differs");need(!tick::authority::active(),"Consumed authority escaped command callback");
    a::Evidence out;need(!rules_observe(fixture_binding,request(),out,e),"Receipt observed outside consumed scope");return digest;
}
void run(){std::string e;reset();auto q=request();go=true;need(rules_queue(fixture_runtime,q.game_rules,e)&&native_calls==0&&design_rules.pending&&design_rules.pending->ordered,"Original-Go capture did not retain pre-store intent");go=false;rules_tick(fixture_runtime);need(submissions==1&&!design_rules.pending&&native_calls==0,"Held capture did not submit exactly once");rules_tick(fixture_runtime);need(submissions==1,"Capture submitted twice");
    for(int drift=0;drift<3;++drift){reset();q=request();need(rules_queue(fixture_runtime,q.game_rules,e),e);if(drift==0)++a::selected.value.session;if(drift==1)binding_fresh=false;if(drift==2)fixture_controller["ordered"]=false;rules_tick(fixture_runtime);need(submissions==0&&fixture_runtime.rejected==1,"Stale captured Session/Binding/mode submitted");}
    reset();q=request();q.game_rules.command=gr::Command::Features;q.game_rules.before.available={1,1};q.game_rules.after=q.game_rules.before;q.game_rules.after.features[0]=1;need(gr::valid(q.game_rules)&&!rules_queue(fixture_runtime,q.game_rules,e)&&!design_rules.pending&&native_calls==0,"Unimplemented valid offered Features family admitted");
    reset();q=request();fixture_controller["pending"]=true;need(rules_perform(fixture_binding,q,nullptr,e)==a::Outcome::RejectedNoEffect&&native_calls==0,"Missing consumed authority reached original");
    const auto host=consumed(true),client=consumed(false);need(!host.empty()&&host==client,"Host and Client complete canonical effects differ");for(int defect=1;defect<=5;++defect){consumed(true,defect);consumed(false,defect);}
    for(auto command:{gr::Command::RespawnSeconds,gr::Command::ApplyXP}){const auto h=consumed(true,0,command),c=consumed(false,0,command);need(!h.empty()&&h==c,"Sibling full canonical proof differs");for(int defect=1;defect<=5;++defect){consumed(true,defect,command);consumed(false,defect,command);}}
    reset();go=true;fixture_ui_fields={{{10000,0},fixture_runtime.image+0x1701d20},{{10000,0x450},30000},{{10000,0x458},20000},{{30000,0},fixture_runtime.image+0x17496b0},{{30000,8},fixture_pin.design+0x1c0},{{30000,0x38},fixture_pin.design+0x1d8},{{20000,0},fixture_runtime.image+0x1703290},{{20000,0x4a0},0},{{fixture_runtime.image,0x13df9e0},fixture_pin.design}};fixture_ui_text={{30000+0x18,"seconds"},{20000+0x2d0,"90"}};
    need(rules_property_entry(reinterpret_cast<void*>(10000),native_command_fixture::record("ValueEdited",{"61"}))&&design_rules.pending&&design_rules.pending->intent.command==gr::Command::RespawnSeconds&&design_rules.pending->intent.after.respawn_seconds==90&&native_calls==0&&fixture_state.state.respawn_seconds==0,"Actual original PropertyPane capture mutated gameplay or lost numeric intent");
    design_rules.pending.reset();fixture_ui_fields[{20000,0x4a0}]=1;need(rules_property_entry(reinterpret_cast<void*>(10000),native_command_fixture::record("ValueEdited",{"61"}))&&!design_rules.pending&&native_calls==0,"Direct-bound editor admitted after unproved early store");
    reset();go=true;fixture_ui_fields={{{10000,0},fixture_runtime.image+0x1703ce0},{{10000+0x338,0},fixture_runtime.image+0x16d9f40}};fixture_ui_draft[1]=20;
    need(rules_xp_entry(reinterpret_cast<void*>(10000),native_command_fixture::record("Apply"))&&design_rules.pending&&design_rules.pending->intent.after.xp==fixture_ui_draft&&fixture_button_calls==1&&native_calls==0&&fixture_state.state.xp[1]==10,"Actual original XP Apply capture changed gameplay or omitted complete draft");
}
#include "rules_numeric_diagnostic_cases.inc"
}
#include "game_rules_ui_cases.inc"
int main()try{fixture::run();fixture::diagnostic_run();rules_ui_fixture::run();rules_ui_fixture::numeric_run();std::cout<<checks<<" ordered Design production body, consumed authority and canonical evidence checks passed\n";return 0;}catch(const std::exception& cause){std::cerr<<cause.what()<<'\n';return 1;}
