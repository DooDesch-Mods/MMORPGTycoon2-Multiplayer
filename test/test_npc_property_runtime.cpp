#include "typed_action/action_session.h"
#include "typed_action/ordered_building_family.h"
#include "ui_remove/ui_capture.h"
#include "npc_census.h"
#include "npc_target_key.h"
#include "release_edit_state.h"
#include <cstring>
#include <iostream>
#include <stdexcept>
using json=nlohmann::json;
namespace action=mt2mp::action_session;
namespace ui=action::ui_capture;
namespace npc_census=mt2mp::npc_census;
namespace npc_target_key=mt2mp::game::npc_target_key;
namespace release_edit=mt2mp::game::release_edit;
bool fail(std::string& error,const std::string& cause){error=cause;return false;}
unsigned checks=0,original_calls=0,seed_calls=0,submit_calls=0,read_calls=0;
void need(bool ok,const std::string& why){++checks;if(!ok)throw std::runtime_error(why);}
const std::string expected_snapshot="22222222-2222-4222-8222-222222222222";
const std::string expected_target="action:33333333-3333-4333-8333-333333333333";
const action::Binding expected_binding{2,3};
action::Context expected_context;
action::Request expected_request;
bool scoped=true,scope_valid=true,original_go=false,binding_valid=true,session_valid=true;
bool mutate_quest=false,mutate_undo=false,mutate_rng=false,mutate_ledger=false;
bool original_returned=true,change_target=false,read_failed=false;
json fixture_controller;
namespace mt2mp::action_session {
struct FixtureSession {bool current(Context& out,std::string& error){if(!session_valid)return fail(error,"Session unavailable");out=::expected_context;return true;}};
FixtureSession& selected_session(){static FixtureSession instance;return instance;}
namespace tick_commands::authority {
bool active(){return scoped;}
bool matches(const Context& actual,const Binding& b,const Request& r,std::string& error){
    return (scoped&&scope_valid&&actual==::expected_context&&b==::expected_binding&&r==::expected_request)||fail(error,"Current consumed command differs");}
bool matches(const Binding& b,const Request& r,std::string& error){return matches(::expected_context,b,r,error);}
}
}
struct Config {bool is_host=true;static Config& instance(){static Config value;return value;}};
unsigned GetCurrentThreadId(){return 7;}
namespace net {std::string current_snapshot_id(){return expected_snapshot;}}
namespace typed_action_runtime {
json controller_status(){return fixture_controller;}
bool original_go_admission(bool& inside,std::string&){inside=original_go;return true;}
bool read_binding(const std::string& id,action::Binding& out,std::string& error){
    if(!binding_valid||id!=expected_snapshot)return fail(error,"Native binding differs");out=expected_binding;return true;}
bool network_binding(const std::string& id,action::Binding& out,std::string& error){
    if(original_go)return fail(error,"Network binding forbidden during original Go");return read_binding(id,out,error);}
bool submit_npc_property(const std::string&,const std::string&,const action::NpcPropertyIntent&,std::string&){++submit_calls;return true;}
}
#include "npc_ordered_admission_actual.inc"
namespace ui_move_runtime {
namespace selection_move {bool history_intent(const action::Binding&,const action::Request&,action::SelectionMoveIntent&,std::string& error){return fail(error,"Unexpected selection route");}}
namespace duplicate_edit {bool history_targets(const action::Binding&,const action::Request&,std::map<uint64_t,std::string>&,std::string& error){return fail(error,"Unexpected duplicate route");}}
namespace group_edit {bool history_admitted(const action::Binding&,const action::Request&,std::string& error){return fail(error,"Unexpected group route");}}
}
struct EngineFixture {
    struct Source {action::Request request;};std::map<std::string,Source> buildings_,operations_;
    bool live_state_=false,live_port_=false,live_result_=false;unsigned routed=0;
    action::Outcome execute_action_native(const action::Binding&,const action::Request&,std::string&){++routed;return action::Outcome::Confirmed;}
    action::Outcome execute(const action::Binding& binding,const action::Request& request,std::string& error){
        #include "ordered_path_route_actual.inc"
        return action::Outcome::RejectedNoEffect;
    }
};
namespace undo {struct UndoWitness {uintptr_t active_graph=0;unsigned count=0;bool operator==(const UndoWitness&)const=default;};}
struct Memory {};
template<class T>bool field(Memory&,uintptr_t base,size_t offset,T& out,std::string& error){
    if constexpr(sizeof(T)==8){if(base==100&&(offset==0x164c5c0||offset==0x164c5e0)){uintptr_t color=200;std::memcpy(&out,&color,8);return true;}}
    if constexpr(sizeof(T)==16){if(base==200&&offset==0){std::array<float,4> color{0.5f,0.25f,0.75f,1};std::memcpy(&out,&color,16);return true;}}
    return fail(error,"Unexpected field read");
}
struct Operation {
    action::Binding binding;action::Request request;action::Evidence evidence;
    uint64_t local_uid=0;uintptr_t local_pointer=0;unsigned attempted=0,returned=0;
    bool confirmed=false,retired=false,ordered=false;std::string cause;
    json quest_tasks_before,quest_tasks_after;
};
struct Runtime {
    bool ready=true,pending=false,capture_failed=false;unsigned thread=7;uintptr_t image=100;
    std::map<std::string,Operation> operations;std::map<std::string,json> ui_jobs;unsigned submits=0;std::string last_action,cause;
} fixture_runtime;
struct PropertyPending {action::NpcPropertyIntent intent;action::Binding binding;std::string snapshot,key;std::optional<action::Context> context;std::string token;};
std::optional<PropertyPending> property_pending;
bool property_installed=true;
action::NpcPropertyState property_state;
action::Ledger ledger;
npc_census::State census_state;
json fixture_tasks=json::array();
undo::UndoWitness fixture_witness;
void rejected(const std::string& cause){fixture_runtime.cause=cause;}
std::string uuid(std::string&){return "11111111-1111-4111-8111-111111111111";}
bool target_binding(const std::string&,const action::Binding& b,const std::string& id,ui::LocalBinding& local,
                    uintptr_t& region,npc_census::State& census,std::string&){
    local={0x5000,uintptr_t(change_target&&original_calls?0x1008:0x1000),77,b.native_epoch,b.native_generation,id,expected_target,false,false,true};
    region=0x2000;census=census_state;return true;
}
bool property_read(Memory&,uintptr_t,uint64_t,uintptr_t,action::NpcPropertyState& out,action::Ledger& money,
                   undo::UndoWitness& undo,npc_census::State& census,json& task,std::string& error){
    ++read_calls;if(read_failed&&original_calls)return fail(error,"Retained native read failure");
    out=property_state;money=ledger;undo=fixture_witness;census=census_state;task=fixture_tasks;return true;
}
uint32_t seed(uintptr_t,const std::array<uint64_t,4>&){++seed_calls;return 0;}
uint32_t property_call(const action::NpcPropertyIntent& intent,uintptr_t,bool& returned){
    ++original_calls;returned=original_returned;if(!returned)return 0;
    if(intent.kind==action::NpcPropertyKind::Name){property_state.actor.created.name=intent.name;property_state.actor.roster_sha256=std::string(64,'b');census_state.rows[0].name=intent.name;}
    else if(intent.kind==action::NpcPropertyKind::OnlineState){property_state.online=intent.online;property_state.force_tint=intent.online!=2;if(property_state.force_tint)property_state.tint={0.5f,0.25f,0.75f,1};}
    else property_state.command_level=action::npc_property_level(property_state,intent.kind);
    if(mutate_quest)fixture_tasks.push_back("unexpected Quest mutation");
    if(mutate_undo)++fixture_witness.count;
    if(mutate_rng){++property_state.actor.global_random[0];++census_state.global_random[0];}
    if(mutate_ledger)++ledger.cash;
    return 0;
}
#define state fixture_runtime
#include "npc_property_census_actual.inc"
#include "npc_property_tick_actual.inc"
#include "npc_property_perform_actual.inc"
#include "npc_property_observe_actual.inc"
#undef state
void reset(action::NpcPropertyKind kind=action::NpcPropertyKind::Name){
    original_calls=seed_calls=submit_calls=read_calls=0;scoped=scope_valid=binding_valid=session_valid=true;original_go=false;
    mutate_quest=mutate_undo=mutate_rng=mutate_ledger=change_target=read_failed=false;original_returned=true;
    fixture_runtime={};property_pending.reset();Config::instance().is_host=true;
    expected_context={};expected_context.role=action::Role::Host;expected_context.snapshot_uuid=expected_snapshot;
    expected_request={};expected_request.action=action::Action::EditNpcProperty;expected_request.action_uuid="11111111-1111-4111-8111-111111111111";
    expected_request.snapshot_uuid=expected_snapshot;expected_request.canonical_record_key=expected_target;expected_request.npc_property.kind=kind;
    expected_request.npc_property.name=kind==action::NpcPropertyKind::Name?"New name":"";expected_request.npc_property.online=kind==action::NpcPropertyKind::OnlineState?1:0;
    fixture_controller={{"ordered",true},{"started",true},{"locked",false},{"pending",true},{"action_uuid",expected_request.action_uuid}};
    property_state={};property_state.online=2;property_state.tint={1,1,1,1};property_state.command_level=5;property_state.region_min=1;property_state.region_max=10;
    auto& actor=property_state.actor;actor.count=1;actor.roster_sha256=std::string(64,'a');actor.created.name="Old name";
    actor.created.type=1;actor.created.character_slot=0;actor.created.character_type=1;actor.created.level=1;actor.created.quest_count=0;
    actor.created.rotation={0,0,0,1};actor.created.scale={1,1,1};
    ledger={};fixture_tasks=json::array();fixture_witness={};census_state={};census_state.manager=1;census_state.next_local_tag=7;
    npc_census::Row row;row.local_uid=77;row.name="Old name";census_state.rows={row};
}
int main(){try{std::string error;
    for(auto kind:{action::NpcPropertyKind::Name,action::NpcPropertyKind::OnlineState,action::NpcPropertyKind::RaiseLevel,action::NpcPropertyKind::LowerLevel}){
        action::Evidence host;
        for(bool is_host:{true,false}){
            reset(kind);Config::instance().is_host=is_host;expected_context.role=is_host?action::Role::Host:action::Role::Client;
            EngineFixture engine;need(engine.execute(expected_binding,expected_request,error)==action::Outcome::Confirmed&&engine.routed==1,error);
            need(property_perform(expected_binding,expected_request,nullptr,error)==action::Outcome::Confirmed,error);
            need(original_calls==1&&seed_calls==0&&fixture_runtime.operations.at(expected_request.action_uuid).ordered,"Original count or ordered RNG ownership differs");
            action::Evidence observed;need(property_observe(expected_binding,expected_request,observed,error),error);
            if(is_host)host=observed;else need(observed==host,"Host/Client complete property effects differ");
            need(property_perform(expected_binding,expected_request,nullptr,error)==action::Outcome::Unknown&&original_calls==1,"Confirmed original replayed");
            scoped=false;need(!property_observe(expected_binding,expected_request,observed,error),"Ordered observation escaped consumed scope");
        }
    }
    auto reject=[&](auto change){reset();change();need(property_perform(expected_binding,expected_request,nullptr,error)==action::Outcome::RejectedNoEffect,error);need(original_calls==0&&seed_calls==0,"Rejected property mutated");};
    reject([]{scoped=false;});reject([]{scope_valid=false;});reject([]{expected_context.role=action::Role::Client;});
    reject([]{fixture_controller["pending"]=false;});reject([]{fixture_controller["locked"]=true;});reject([]{fixture_runtime.thread=8;});
    reject([]{fixture_controller["action_uuid"]="foreign";});reject([]{property_state.actor.created.name="New name";});
    reset();action::Evidence authority;need(property_perform(expected_binding,expected_request,&authority,error)==action::Outcome::RejectedNoEffect&&seed_calls==0&&original_calls==0,"Ordered Host state/RNG adopted");
    for(auto fault:{&mutate_quest,&mutate_undo,&mutate_rng,&mutate_ledger,&change_target,&read_failed}){
        reset();*fault=true;need(property_perform(expected_binding,expected_request,nullptr,error)==action::Outcome::Unknown,"Partial effect fault accepted");
        need(original_calls==1&&seed_calls==0,"Fault original count differs");
        need(property_perform(expected_binding,expected_request,nullptr,error)==action::Outcome::Unknown&&original_calls==1,"Unknown effect replayed");
    }
    reset();original_returned=false;need(property_perform(expected_binding,expected_request,nullptr,error)==action::Outcome::Unknown&&original_calls==1,"Original failed return accepted");
    reset();original_go=true;action::Binding captured;need(capture_binding(expected_snapshot,captured,error)&&captured==expected_binding,error);
    std::optional<action::Context> captured_context;need(capture_context(expected_snapshot,expected_binding,captured_context,error)&&captured_context==expected_context,error);
    original_go=false;property_pending=PropertyPending{expected_request.npc_property,expected_binding,expected_snapshot,expected_target,captured_context};property_tick();
    need(submit_calls==1&&fixture_runtime.submits==1&&!fixture_runtime.capture_failed,"Exact captured command did not submit");
    reset();property_pending=PropertyPending{expected_request.npc_property,expected_binding,expected_snapshot,expected_target,expected_context};expected_context.role=action::Role::Client;property_tick();
    need(submit_calls==0&&fixture_runtime.capture_failed&&fixture_runtime.cause=="Ordered NPC captured Session changed","Changed captured Context submitted");
    reset();EngineFixture engine;engine.live_state_=true;need(engine.execute(expected_binding,expected_request,error)==action::Outcome::RejectedNoEffect&&engine.routed==0,"Ordered live-state bridge admitted");
    std::cout<<checks<<" actual ordered NPC property execution/capture/route cases passed\n";return 0;
}catch(const std::exception& exception){std::cerr<<exception.what()<<'\n';return 1;}}
