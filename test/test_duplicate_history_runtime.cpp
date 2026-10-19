#define main duplicate_protocol_fixture_main
#include "test_duplicate_protocol.cpp"
#undef main
#include "duplicate_history_reader.h"
#include "move_contract.h"
#include "building_npc_lifecycle.h"
#include "duplicate_history_effects.h"
#include <atomic>
namespace history_fixture {
namespace action=mt2mp::action_session;namespace duplicate_history=mt2mp::game::duplicate_history;
namespace q=mt2mp::game::npc_quest_targets;namespace contract=mt2mp::game::ui_move_runtime::contract;
namespace monster_quest_retirement=mt2mp::game::monster_quest_retirement;
namespace duplicate_quest_history=mt2mp::game::duplicate_quest_history;
namespace duplicate_history_effects=mt2mp::game::duplicate_history_effects;
namespace child_npc {
using Receipt=mt2mp::game::building_npc_lifecycle::Receipt;
using Parent=mt2mp::game::building_npc_lifecycle::Parent;
using Lease=mt2mp::game::building_npc_lifecycle::Lease;
}
struct HistoryServices {mt2mp::game::duplicate_service_history::World saved;};
struct HistoryBuildingPlan {
    HistoryServices services;monster_quest_retirement::Witness quests;
    mt2mp::game::duplicate_service_history::Removal service_removal;
    std::set<std::string> related_keys;std::vector<uintptr_t> quest_owners;
    std::map<uint64_t,child_npc::Receipt> children;
};
using Family=action::DuplicateFamily;using json=nlohmann::json;
struct Handle{uintptr_t object=0;uint64_t uid=0;bool operator==(const Handle&)const=default;};
struct Sample{unsigned generation=4,undo=1;std::map<std::string,Handle> handles;DuplicateState state;Ledger money;};
struct Runtime{bool ready=true;uint32_t thread=17;uintptr_t image=100;int memory=0;};
struct Operation{Binding binding;Request request;Sample before,after;bool confirmed=true,retired=true;std::map<uint64_t,std::string> created;};
#include "duplicate_history_structs_actual.inc"
Runtime fixture_runtime;std::atomic<Runtime*> published{&fixture_runtime};std::map<std::string,Operation> operations;
const Binding fixture_binding{3,4};Sample fixture_state;json fixture_controller;std::string fault;
unsigned original_calls=0,restores=0,public_binding_calls=0,alias_changes=0;uintptr_t graph_current=1;
struct Config{bool is_host=true;static Config& instance(){static Config c;return c;}};
uint32_t GetCurrentThreadId(){return 17;}
namespace net{std::string current_snapshot_id(){return fixture_snapshot_uuid;}}
namespace typed_action_runtime{
json controller_status(){return fixture_controller;}
bool network_binding(const std::string&,Binding&,std::string& e){++public_binding_calls;e="public idle admission rejects pending";return false;}
}
namespace duplicate_code{bool check(int,uintptr_t,std::string& e){return fault!="code"||q::fail(e,"native body changed");}}
bool same_binding(const Sample& first,const Sample& second){return first.generation==second.generation;}
bool sample(Runtime&,const Binding& binding,const DuplicateIntent*,const std::map<uint64_t,std::string>&,Sample& out,std::string& e,
            const std::function<bool(Sample&,std::string&)>& prepare={}){
    if(binding!=fixture_binding||fixture_state.generation!=binding.native_generation)return q::fail(e,"native current generation changed");
    if(fault=="after-read"&&original_calls)return q::fail(e,"actual after read failed");out=fixture_state;return !prepare||prepare(out,e);}
bool history_known(Runtime&,const Sample& value,HistorySource& source,std::string& e){
    if(fault=="recreated-uid"&&original_calls)return q::fail(e,"original recreation full UID differs");
    source.last_live=value.state;return true;}
bool history_graph(Runtime&,uintptr_t,bool,const HistorySource&,const DuplicateIntent&,std::string& e){
    if(fault=="graph"&&!original_calls)return q::fail(e,"first original graph ownership cause");
    if(fault=="inverse"&&original_calls)return q::fail(e,"original nested inverse changed");return true;}
bool history_local_selection(Runtime&,const Sample&,std::vector<Handle>&,std::array<uint8_t,9>&,uintptr_t&,std::string& e){return fault!="selection"||q::fail(e,"local selection unavailable");}
bool history_restore(Runtime&,const Sample&,const std::vector<Handle>&,const std::array<uint8_t,9>&,uintptr_t,const HistorySource&,std::string& e){
    ++restores;return (fault!="restore"&&fault!="original-restore")||q::fail(e,"local selection cleanup failed");}
namespace remove_diagnostics{
bool retire_undone_building(const std::string&,const std::string&,uint64_t,std::string& e){++alias_changes;return fault!="alias"||q::fail(e,"actual target absence not confirmed");}
bool restore_redone_building(const std::string&,const std::string&,uint64_t,std::string& e){++alias_changes;return fault!="alias"||q::fail(e,"actual target restoration not confirmed");}
}
namespace building_history_slice{
bool graph_for(Action,const std::string&,uintptr_t& out,std::string& e){out=graph_current;return fault!="stack"||q::fail(e,"original graph not stack top");}
Outcome perform(Action direction,const std::string&,std::string& e){
    if(fault=="bridge-reject"){e="original history delayed";return Outcome::RejectedNoEffect;}
    ++original_calls;if(fault=="original"||fault=="original-restore"){e="first original graph failure";return Outcome::Unknown;}
    const bool undo=direction==Action::UndoBuilding;const auto& birth=operations.at(fixture_action_uuid);
    auto money=fixture_state.money;const int32_t amount=undo?10:-10;
    money.cash+=amount;money.record_cash+=amount;money.record_income+=amount;money.line_income[6]+=amount;money.line_counts[6]++;
    fixture_state=birth.after;fixture_state.money=money;
    if(undo){fixture_state.state.created.clear();fixture_state.handles.erase(fresh(0));}
    else fixture_state.handles.at(fresh(0)).object=3000;
    graph_current=undo?2:1;fixture_state.undo=unsigned(graph_current);
    if(fault=="survivor-uid")fixture_state.handles.at(key(1)).uid++;
    if(fault=="record")fixture_state.state.sources[0].record_sha=std::string(64,'f');
    if(fault=="unrelated")fixture_state.state.untouched_sha=std::string(64,'f');
    if(fault=="cash")fixture_state.money.record_expense++;
    if(fault=="release")fixture_state.state.release[0]++;
    return Outcome::Confirmed;
}
}
unsigned child_retirements=0,child_restorations=0,building_graph_checks=0;
std::map<uint64_t,child_npc::Receipt> child_current;
child_npc::Lease child_lease(const Binding& binding,const Request& req,const std::string& original){
    return {req.snapshot_uuid,req.action_uuid,original,binding.native_epoch,binding.native_generation};}
bool external_owner(Runtime&,const Sample&,uintptr_t,std::string& key,std::string&){key="root_shard";return true;}
bool child_parent(Runtime&,const Sample& value,const std::string& key,child_npc::Parent& out,std::string& e){
    const auto found=value.handles.find(key);if(found==value.handles.end())return q::fail(e,"restored parent absent");
    out={};out.key=key;out.uid=found->second.uid;out.object=found->second.object;out.saved_region=0;return true;}
bool child_alias(Sample&,const child_npc::Receipt&,std::string&){return true;}
namespace child_npc {
bool confirm_retired(const Lease&,const Receipt& before,std::string& e){++child_retirements;
    if(fault=="child-retire")return q::fail(e,"original child UID remains after queued deletion");
    child_current.erase(before.parent.uid);return true;}
template<class Owner>bool confirm_restored(const Lease& lease,const Receipt& before,const Parent& parent,Owner&&,Receipt& out,std::string& e){
    ++child_restorations;if(fault=="child-restore")return q::fail(e,"restored child reciprocal proof differs");
    out=before;out.lease=lease;out.parent=parent;out.child.row.local_pointer=9000;
    if(fault=="child-record")out.original_record_sha=std::string(64,'e');child_current[parent.uid]=out;return true;}
template<class Owner>bool capture_before(const Lease&,const Parent& parent,Owner&&,Receipt& out,std::string& e){
    const auto found=child_current.find(parent.uid);if(found==child_current.end())return q::fail(e,"current child receipt absent");out=found->second;return true;}
}
bool history_building_plan(Runtime&,const Binding&,const Sample&,uintptr_t,const std::map<uint64_t,duplicate_history::Known>&,
                           const Request& request,HistoryBuildingPlan& out,std::string& e){
    if(fault=="building-plan")return q::fail(e,"complete Building inverse preflight failed");
    out={};out.services.saved={{100,{}},{200,{}}};out.service_removal.remaining={{100,{}}};out.service_removal.survivors={{100,{}}};
    child_npc::Receipt child;child.lease=child_lease(fixture_binding,request,request.target_action_uuid);
    child.parent={fresh(0),2000,200,0};child.present=true;child.original_record_sha=std::string(64,'c');
    child.child.key=fresh(0)+"/npc";child.child.row.local_pointer=8000;child.child.row.local_uid=800;
    out.children={{200,child}};child_current=out.children;out.quest_owners={8000};
    out.quests.manager=88;monster_quest_retirement::Owner owner;owner.pointer=8000;owner.uid=800;owner.key=child.child.key;
    out.quests.owners={{8000,owner}};return true;
}
bool history_building_graph(Runtime&,uintptr_t,const std::map<uint64_t,duplicate_history::Known>&,const child_npc::Lease&,
                            HistoryBuildingPlan&,std::vector<uintptr_t>&,std::string& e){
    ++building_graph_checks;return fault!="building-inverse"||q::fail(e,"complete original MakeNPC inverse differs");}
bool history_service_world(Runtime&,const Sample& value,HistoryServices& out,std::string& e){
    out.saved={{100,{}}};if(!value.state.created.empty())out.saved[200]={};return fault!="service-read"||q::fail(e,"complete Service census failed");}
namespace npc_probe::world_quest_history {
bool read(const Binding&,const std::string&,monster_quest_retirement::Witness& out,std::string& e,
          const monster_quest_retirement::Witness* previous=nullptr){
    out=previous?*previous:monster_quest_retirement::Witness{};out.manager=88;
    return fault!="quest-read"||q::fail(e,"complete Quest census failed");}
}
namespace group_edit {bool find(Runtime&,uint64_t uid,uintptr_t& object,std::string&){object=uid*10;return true;}}
namespace duplicate_service_history {
bool absent(const mt2mp::game::duplicate_service_history::Removal& expected,const mt2mp::game::duplicate_service_history::World& actual,std::string& e){
    return mt2mp::game::duplicate_service_history::absent(expected,actual,e);}
template<class Find>bool restored(int,uintptr_t,uintptr_t,uintptr_t,const mt2mp::game::duplicate_service_history::Saved&,Find&&,std::string&){return true;}
}
std::vector<duplicate_history_effects::OwnedChild> history_children(const std::map<uint64_t,child_npc::Receipt>& children,bool retired){
    std::vector<duplicate_history_effects::OwnedChild> out;for(const auto& [uid,child]:children){(void)uid;
        out.push_back({child.parent.key,retired?"":child.child.key,retired?"":child.original_record_sha,child.present&&!retired});}return out;}
bool history_effect_witness(const Sample&,const HistoryServices& services,const monster_quest_retirement::Witness& quests,
                           const std::set<std::string>& related,const std::vector<duplicate_history_effects::OwnedChild>& children,
                           DuplicateState& state,std::string& e){
    action::DuplicateHistoryEffects effects;effects.related_keys.assign(related.begin(),related.end());
    if(!duplicate_history_effects::digest(services.saved.size(),effects.services_sha,e)||
       !duplicate_history_effects::quests(quests,effects.quests_sha,e)||
       !duplicate_history_effects::owned_children(std::string(64,'d'),children,effects.related_sha,e))return false;
    state.history=std::move(effects);return true;}
#include "duplicate_history_survivors_actual.inc"
#include "duplicate_history_perform_actual.inc"
Request request(Action direction=Action::UndoBuilding){Request r;r.action=direction;r.peer=2;r.sequence=1;
    r.action_uuid="33333333-3333-4333-8333-333333333333";r.snapshot_uuid=fixture_snapshot_uuid;r.target_action_uuid=fixture_action_uuid;r.canonical_record_key="duplicate:"+fixture_action_uuid;return r;}
void reset(){fixture_runtime={};history_sources.clear();history_operations.clear();operations.clear();original_calls=restores=public_binding_calls=alias_changes=0;graph_current=1;fault.clear();Config::instance().is_host=true;
    child_retirements=child_restorations=building_graph_checks=0;child_current.clear();
    auto m=message();m.request.duplicate.rows[0].family=Family::Scenery;m.evidence.duplicate_before->sources[0].family=Family::Scenery;
    m.evidence.duplicate_after->sources[0].family=Family::Scenery;m.evidence.duplicate_after->created[0].family=Family::Scenery;
    auto& birth=operations[fixture_action_uuid];birth.binding=fixture_binding;birth.request=m.request;birth.created={{200,fresh(0)}};
    birth.after.state=*m.evidence.duplicate_after;birth.after.money=m.evidence.after;birth.after.handles={{key(1),{1000,100}},{fresh(0),{2000,200}}};fixture_state=birth.after;
    auto& source=history_sources[fixture_action_uuid];source.last_live=fixture_state.state;duplicate_history::Known known;
    known.uid=200;known.object=2000;known.row=fixture_state.state.created[0];source.known[200]=known;
    fixture_controller={{"started",true},{"pending",true},{"locked",false},{"action_uuid",request().action_uuid},{"host_revision",uint64_t(2)}};
}
void building(){reset();auto& birth=operations.at(fixture_action_uuid);birth.request.duplicate.rows[0].family=Family::Building;
    birth.after.state.sources[0].family=Family::Building;birth.after.state.created[0].family=Family::Building;
    fixture_state=birth.after;auto& source=history_sources.at(fixture_action_uuid);source.last_live=fixture_state.state;
    source.known.at(200).row=fixture_state.state.created[0];}
void run(){std::string e;reset();const auto req=request();Evidence host;
    need(perform_history(fixture_binding,req,nullptr,e)==Outcome::Confirmed,e);
    need(observe_history(fixture_binding,req,host,e)&&retire_history(fixture_binding,req,e)&&original_calls==1&&alias_changes==1&&!public_binding_calls,e);
    need(!retire_history(fixture_binding,req,e),"history retired twice");
    need(perform_history(fixture_binding,req,nullptr,e)==Outcome::Unknown&&original_calls==1,"confirmed graph replayed");
    auto redo=request(Action::RedoBuilding);redo.action_uuid="44444444-4444-4444-8444-444444444444";fixture_controller["action_uuid"]=redo.action_uuid;fixture_controller["host_revision"]=uint64_t(3);
    need(perform_history(fixture_binding,redo,nullptr,e)==Outcome::Confirmed&&original_calls==2&&alias_changes==2,e);
    need(fixture_state.handles.at(fresh(0)).uid==200&&fixture_state.money.record_expense==10,"Redo lost UID or treated negative cash as expense");
    reset();Config::instance().is_host=false;need(perform_history(fixture_binding,req,&host,e)==Outcome::Confirmed&&original_calls==1&&!public_binding_calls,e);
    auto rejected=[&](auto change){reset();auto r=req;change(r);need(perform_history(fixture_binding,r,nullptr,e)==Outcome::RejectedNoEffect&&!original_calls,"invalid history mutated original graph");};
    rejected([](auto&){fixture_controller["pending"]=false;});rejected([](auto&){fixture_controller["locked"]=true;});
    rejected([](auto&){fixture_controller["action_uuid"]=fixture_snapshot_uuid;});rejected([](auto&){fixture_runtime.thread=18;});
    rejected([](auto&){Config::instance().is_host=false;});rejected([](auto&){fixture_state.generation++;});
    rejected([](auto& r){r.canonical_record_key="duplicate:"+r.action_uuid;});rejected([](auto&){operations.at(fixture_action_uuid).retired=false;});
    rejected([](auto&){history_sources.clear();});
    rejected([](auto&){history_sources.at(fixture_action_uuid).known.at(200).row.family=Family::Building;fault="building-plan";});
    for(const std::string stage:{"code","graph","selection","stack","bridge-reject"})rejected([&](auto&){fault=stage;});
    reset();Config::instance().is_host=false;auto bad=host;bad.duplicate_before->random[0]++;
    need(perform_history(fixture_binding,req,&bad,e)==Outcome::RejectedNoEffect&&!original_calls,"different Host prior accepted");
    for(const std::string stage:{"original","original-restore","after-read","survivor-uid","record","unrelated","cash","release","inverse","alias","restore"}){
        reset();fault=stage;need(perform_history(fixture_binding,req,nullptr,e)==Outcome::Unknown&&original_calls==1&&!e.empty(),"partial graph not Unknown");
        if(stage=="original-restore")need(e.starts_with("first original graph failure; cleanup:"),"first original cause lost");
        need(perform_history(fixture_binding,req,nullptr,e)==Outcome::Unknown&&original_calls==1,"partial graph replayed");}
    reset();need(perform_history(fixture_binding,req,nullptr,e)==Outcome::Confirmed,e);fixture_state.money.cash++;
    need(!observe_history(fixture_binding,req,host,e),"changed actual Ledger ACKed");
    reset();need(perform_history(fixture_binding,req,nullptr,e)==Outcome::Confirmed,e);fixture_state.handles.at(key(1)).uid++;
    need(!observe_history(fixture_binding,req,host,e),"changed survivor full UID ACKed");
    building();Evidence building_host;
    need(perform_history(fixture_binding,req,nullptr,e)==Outcome::Confirmed&&child_retirements==1&&building_graph_checks==1,e);
    need(observe_history(fixture_binding,req,building_host,e)&&retire_history(fixture_binding,req,e)&&building_host.duplicate_after->history.has_value(),e);
    need(history_operations.at(req.action_uuid).quests.owners.empty(),"retired child Quest pointer was reused in actual after read");
    fixture_controller["action_uuid"]=redo.action_uuid;fixture_controller["host_revision"]=uint64_t(3);
    need(perform_history(fixture_binding,redo,nullptr,e)==Outcome::Confirmed&&child_restorations==1&&building_graph_checks==2,e);
    need(history_operations.at(redo.action_uuid).quests.owners.contains(9000)&&
        !history_operations.at(redo.action_uuid).quests.owners.contains(8000),"original Redo Quest owner pointer was not explicitly remapped");
    need(observe_history(fixture_binding,redo,host,e),e);
    building();Config::instance().is_host=false;
    need(perform_history(fixture_binding,req,&building_host,e)==Outcome::Confirmed&&child_retirements==1,e);
    for(const std::string stage:{"building-inverse","child-retire","service-read","quest-read"}){building();fault=stage;
        need(perform_history(fixture_binding,req,nullptr,e)==Outcome::Unknown&&original_calls==1&&!e.empty(),"partial Building original graph not Unknown");
        need(perform_history(fixture_binding,req,nullptr,e)==Outcome::Unknown&&original_calls==1,"partial Building original graph replayed");}
    building();need(perform_history(fixture_binding,req,nullptr,e)==Outcome::Confirmed,e);
    fixture_controller["action_uuid"]=redo.action_uuid;fixture_controller["host_revision"]=uint64_t(3);fault="building-inverse";
    need(perform_history(fixture_binding,redo,nullptr,e)==Outcome::RejectedNoEffect&&original_calls==1,"changed stored MakeNPC inverse entered Redo");
    building();need(perform_history(fixture_binding,req,nullptr,e)==Outcome::Confirmed,e);
    fixture_controller["action_uuid"]=redo.action_uuid;fixture_controller["host_revision"]=uint64_t(3);fault="child-restore";
    need(perform_history(fixture_binding,redo,nullptr,e)==Outcome::Unknown&&original_calls==2,"failed restored reciprocal child was ACKed");
}
}
int main()try{history_fixture::run();std::cout<<"Actual Duplicate whole graph pending Host/Client/signed cash/inverse/alias/ACK/failure/no replay cases passed\n";return 0;}
catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}
