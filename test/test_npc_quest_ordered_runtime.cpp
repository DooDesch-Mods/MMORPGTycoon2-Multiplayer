#include "npc_quest_list_fixture.h"
#include "npc_quest_list_read.h"
#include "typed_action/tick_commands.h"
#include "typed_action/ordered_building_family.h"
#include "ui_remove/ui_capture.h"
#include <iostream>
#include <optional>

namespace a=mt2mp::action_session;
namespace action=a;
namespace release_edit=mt2mp::game::release_edit;
namespace qr=mt2mp::game::monster_quest_retirement;
namespace npc_quest_list_read=mt2mp::game::npc_quest_list_read;
namespace q=mt2mp::npc_quest_records;
using json=nlohmann::json;
namespace fixture {
bool fail(std::string& e,const std::string& why){e=why;return false;}
const a::Binding fixture_binding{3,4};
a::Context fixture_context;
a::Request current_request;
bool consumed=true,scope_valid=true,inside_go=false,binding_valid=true,session_valid=true;
bool mutated=false,change_task=false,change_ledger=false,change_region_rng=false,change_references=false,change_survivor=false,change_global_rng=false;
bool original_returned=true,preparation_changed=false;
unsigned originals=0,seeds=0,submits=0;
json fixture_controller;
unsigned GetCurrentThreadId(){return 7;}
struct Config{bool is_host=true;static Config& instance(){static Config value;return value;}};
namespace session_port {
struct Session{bool current(a::Context& out,std::string& e){if(!session_valid)return fail(e,"Session lost");out=fixture_context;return true;}};
}
}
namespace mt2mp::action_session {
fixture::session_port::Session& selected_session(){static fixture::session_port::Session value;return value;}
namespace tick_commands::authority {
bool active(){return fixture::consumed;}
bool matches(const Context& context,const Binding& binding,const Request& request,std::string& e){
    return (fixture::consumed&&fixture::scope_valid&&context==fixture::fixture_context&&binding==fixture::fixture_binding&&request==fixture::current_request)||fixture::fail(e,"Consumed scope differs");}
bool matches(const Binding& binding,const Request& request,std::string& e){return matches(fixture::fixture_context,binding,request,e);}
}
}
namespace fixture {
namespace ui_move_runtime {
namespace selection_move{bool history_intent(const a::Binding&,const a::Request&,a::SelectionMoveIntent&,std::string& e){return fail(e,"Unexpected selection history");}}
namespace duplicate_edit{bool history_targets(const a::Binding&,const a::Request&,std::map<uint64_t,std::string>&,std::string& e){return fail(e,"Unexpected duplicate history");}}
namespace group_edit{bool history_admitted(const a::Binding&,const a::Request&,std::string& e){return fail(e,"Unexpected group history");}}
}
struct EngineFixture{
    struct Source{a::Request request;};std::map<std::string,Source> buildings_,operations_;
    bool live_state_=false,live_port_=false,live_result_=false;unsigned routed=0;
    a::Outcome execute_action_native(const a::Binding&,const a::Request&,std::string&){++routed;return a::Outcome::Confirmed;}
    a::Outcome execute(const a::Binding& binding,const a::Request& request,std::string& error){
        #include "ordered_path_route_actual.inc"
        return a::Outcome::RejectedNoEffect;
    }
};
namespace typed_action_runtime {
json controller_status(){return fixture_controller;}
bool original_go_admission(bool& inside,std::string&){inside=inside_go;return true;}
bool read_binding(const std::string& snapshot,a::Binding& out,std::string& e){
    if(!binding_valid||snapshot!=snapshot_id)return fail(e,"Binding differs");out=fixture_binding;return true;}
bool network_binding(const std::string& snapshot,a::Binding& out,std::string& e){
    if(inside_go)return fail(e,"Network binding inside original Go");return read_binding(snapshot,out,e);}
bool submit_npc_quest_list(const std::string&,const std::string&,const a::NpcQuestListIntent&,std::string&){++submits;return true;}
}
namespace net{std::string current_snapshot_id(){return snapshot_id;}}
struct Memory{};
struct World{uintptr_t root=10;};
struct Pin{World world;};
namespace d {
struct Sample{
    struct State{std::array<uint64_t,4> global_random{1,2,3,4},region_random{5,6,7,8};bool operator==(const State&)const=default;}state;
    struct Undo{uintptr_t active_graph=0;std::vector<uintptr_t> undo;}undo;
    a::Ledger money;unsigned release_edits=0;
};
}
namespace action_runtime {
namespace ui=a::ui_capture;
std::string uuid(std::string&){return action_id;}
uint32_t seed(uintptr_t,const std::array<uint64_t,4>&){++seeds;return 0;}
}
struct Pending{a::Binding binding;std::string snapshot,owner,token;a::NpcQuestListIntent intent;std::optional<a::Context> context;};
struct References{uintptr_t manager=20,data=30;std::vector<std::array<uintptr_t,3>> local;json rows=json::array();};
References fixture_references;
std::string fixture_target_pool="pool";
bool references(Memory&,const a::Binding&,const std::string&,const qr::Witness&,References& out,std::string&){out=fixture_references;return true;}
struct Operation{
    a::Binding binding;a::Request request;Pin pin;uintptr_t owner=0,graph=0;
    qr::Witness before,after;References refs_before,refs_after;d::Sample sample;a::Evidence evidence;
    bool attempted=false,returned=false,seed_attempted=false,confirmed=false,retired=false,ordered=false;
    unsigned calls=0;std::string cause;
};
struct Runtime{uintptr_t image=100;unsigned thread=7;}state;
bool ready=true,fault=false,replaying=false,replay_remove=false;
uintptr_t replay_receiver=0;
a::QuestListEdit replay_edit=a::QuestListEdit::List;
unsigned original_calls=0,submitted=0;
std::string last_action;
std::optional<Pending> pending;
std::map<std::string,Operation> operations;
std::map<std::string,json> jobs;
bool reject(const std::string& e){return fail(last_action,e);}
#include "quest_list_scope_actual.inc"
namespace quest_bridge {
namespace quest=q;
#include "quest_list_records_actual.inc"
bool sha256(const std::string& text,std::string& out,std::string&){out=text;return true;}
bool pool(int32_t region,std::string& out,std::string& e){if(region!=67)return fail(e,"Wrong target Region");out=fixture_target_pool;return true;}
}
#include "quest_list_digest_actual.inc"
#include "quest_list_authoring_effect_actual.inc"
bool same_world(const World& x,const World& y){return x.root==y.root;}
a::Ledger ledger(const a::Ledger& value){return value;}
namespace guards{constexpr struct{uintptr_t rva;}npc{1},selector{2};}
bool inner_codes(Memory&,std::string&){return true;}
bool verify_ui(uintptr_t,bool,std::string&){return true;}
bool bind_owner(Memory&,const a::Binding& actual,const std::string& snapshot,const std::string& key,int32_t region,action_runtime::ui::LocalBinding& local,Pin&,std::string& e){
    if(actual!=fixture_binding||snapshot!=snapshot_id||key!=owner_key||region!=67)return fail(e,"Owner binding differs");local.object=5000;return true;}
qr::Witness world(){
    qr::Witness out;out.manager=100;out.completed=3;
    for(int i=0;i<3;++i){const auto pointer=uintptr_t(1000+i*100),owner=uintptr_t(5000);
        const auto row=record(i);out.quests.push_back(pointer);out.rows.push_back({owner_key,row});out.links.push_back({7000,8000,7032,owner,9000,5032});
        auto& saved=out.owners[owner];saved.pointer=owner;saved.uid=88;saved.key=owner_key;saved.state.records.push_back(row);
        saved.state.local.push_back({pointer,owner,7000,true,true,true,true,q::TaskStatus::Absent});saved.destination_uids.push_back(77);
    }
    auto unrelated=record(0);unrelated.name="Unrelated NPC Quest";unrelated.completed={};
    const std::string unrelated_key="action:66666666-6666-4666-8666-666666666666";
    out.quests.push_back(2000);out.rows.push_back({unrelated_key,unrelated});out.links.push_back({7000,8000,7032,6000,9000,6032});
    auto& other=out.owners[6000];other.pointer=6000;other.uid=99;other.key=unrelated_key;other.state.records.push_back(unrelated);
    other.state.local.push_back({2000,6000,7000,true,true,true,true,q::TaskStatus::Absent});other.destination_uids.push_back(77);
    return out;
}
qr::Witness changed(qr::Witness out){
    const auto& intent=current_request.npc_quest_list;auto& owner=out.owners.at(5000);
    if(intent.edit==a::QuestListEdit::Title){out.rows[size_t(intent.remove_slot)].record.name=intent.title;owner.state.records[size_t(intent.remove_slot)].name=intent.title;}
    else if(intent.edit==a::QuestListEdit::Kills||intent.edit==a::QuestListEdit::Rewards){
        auto& row=owner.state.records[size_t(intent.remove_slot)];
        if(intent.edit==a::QuestListEdit::Kills)row.monsters_to_kill=intent.kills;
        else{row.xp_reward=intent.defaults?-1:intent.xp;row.gold_reward=intent.defaults?-1:intent.gold;}
        out.rows[size_t(intent.remove_slot)].record=row;}
    else if(intent.remove_slot==-1){auto row=record(3);row.accepted={};row.completed={};row.rating={};
        out.quests.push_back(1300);out.rows.push_back({owner_key,row});out.links.push_back({7000,8000,7032,5000,9000,5032});owner.state.records.push_back(row);
        owner.state.local.push_back({1300,5000,7000,true,true,true,true,q::TaskStatus::Pending});owner.destination_uids.push_back(77);}
    else{const auto slot=size_t(intent.remove_slot);const auto at=size_t(std::find(out.quests.begin(),out.quests.end(),owner.state.local[slot].quest)-out.quests.begin());
        out.quests[at]=out.quests.back();out.rows[at]=out.rows.back();out.links[at]=out.links.back();out.quests.pop_back();out.rows.pop_back();out.links.pop_back();
        owner.state.records.erase(owner.state.records.begin()+slot);owner.state.local.erase(owner.state.local.begin()+slot);owner.destination_uids.erase(owner.destination_uids.begin()+slot);--out.completed;
        for(size_t i=0;i<owner.state.records.size();++i){owner.state.records[i].sequence_id=int32_t(i);
            const auto global=size_t(std::find(out.quests.begin(),out.quests.end(),owner.state.local[i].quest)-out.quests.begin());out.rows[global].record.sequence_id=int32_t(i);}}
    if(change_task)owner.state.local.front().task=q::TaskStatus::Pending;
    if(change_survivor){owner.state.records[1].name="Unexpected survivor edit";out.rows[1].record.name=owner.state.records[1].name;}
    return out;
}
bool state_read(Memory&,const a::Binding&,const std::string&,const Pin&,const action_runtime::ui::LocalBinding&,const std::string&,qr::Witness& witness,References& refs,d::Sample& sample,a::NpcQuestListState& value,std::string&,const qr::Witness* previous=nullptr){
    witness=mutated?changed(world()):world();if(previous)witness.removed=previous->removed;
    if(mutated&&current_request.npc_quest_list.edit==a::QuestListEdit::List&&current_request.npc_quest_list.remove_slot>=0)witness.removed.clear();
    value=before();value.records=witness.owners.at(5000).state.records;value.count=uint32_t(witness.quests.size());value.completed=witness.completed;
    value.all_sha256=std::string(64,mutated?'b':'a');value.undo_count=mutated?1:0;sample={};refs={};
    if(mutated){sample.undo.undo={50};if(current_request.npc_quest_list.edit==a::QuestListEdit::List&&current_request.npc_quest_list.remove_slot==-1)++value.global_random[0];
        if(change_ledger)++sample.money.cash;if(change_region_rng)++sample.state.region_random[0];if(change_references)refs.rows.push_back("changed");
        if(change_global_rng)++value.global_random[0];}
    if(preparation_changed&&!mutated)++value.global_random[0];
    return true;
}
bool supported(const a::NpcQuestListIntent& intent,const a::NpcQuestListState& value,std::string& e){return intent.before==value.records||fail(e,"Before differs");}
bool inverse(Memory&,const Operation&,const d::Sample&,std::string&){return true;}
namespace mt2fixture{bool absent=true;}
}
namespace mt2mp::game::monster_quest_retirement {
inline bool absent(fixture::Memory&,uintptr_t,const Witness&,std::string& e){return fixture::mt2fixture::absent||fixture::fail(e,"Removed Quest remains");}
}
namespace mt2mp::action_session::ui_diagnostic {
struct Api{};struct Window{uintptr_t window=6000;};struct Receipt{bool ui_command_returned=false,record_cleanup_confirmed=false;};
struct Calls{bool command(const Api&,const Window& window,Receipt& receipt,std::string&){
    need(fixture::replaying&&fixture::replay_receiver==window.window,"Original transaction scope missing");
    ++fixture::originals;++fixture::original_calls;fixture::mutated=true;receipt.ui_command_returned=fixture::original_returned;receipt.record_cleanup_confirmed=true;return fixture::original_returned;}};
Calls& process_calls(){static Calls calls;return calls;}
}
namespace fixture {
bool prepare_command(Memory&,const a::Binding&,const action_runtime::ui::LocalBinding&,const qr::Witness&,const a::NpcQuestListIntent&,a::ui_diagnostic::Api&,a::ui_diagnostic::Window&,std::string&){return true;}
bool no_gameplay_change(const d::Sample& before,const d::Sample& after){return before.state==after.state&&before.money==after.money;}
#include "quest_list_transition_actual.inc"
#include "quest_list_runtime_actual.inc"
#include "quest_list_tick_actual.inc"
#include "quest_list_detail_actual.inc"
void reset(bool host=true,a::QuestListEdit edit=a::QuestListEdit::List,int slot=-1){
    fixture_context={};fixture_context.session=5;fixture_context.snapshot_uuid=snapshot_id;fixture_context.role=host?a::Role::Host:a::Role::Client;fixture_context.local_peer=host?1:2;fixture_context.host_peer=1;fixture_context.peer_mask=3;
    Config::instance().is_host=host;current_request=request().request;current_request.npc_quest_list={67,slot,world().owners.at(5000).state.records};
    current_request.npc_quest_list.edit=edit;if(edit!=a::QuestListEdit::List)current_request.action=a::Action::EditNpcQuest;
    if(edit==a::QuestListEdit::Title)current_request.npc_quest_list.title="New title";
    if(edit==a::QuestListEdit::Kills)current_request.npc_quest_list.kills=17;
    if(edit==a::QuestListEdit::Rewards){current_request.npc_quest_list.xp=400;current_request.npc_quest_list.gold=31;}
    consumed=scope_valid=binding_valid=session_valid=original_returned=true;inside_go=mutated=change_task=change_ledger=change_region_rng=change_references=change_survivor=change_global_rng=preparation_changed=false;
    ready=true;fault=false;operations.clear();pending.reset();jobs.clear();originals=seeds=submits=submitted=0;
    fixture_controller={{"started",true},{"locked",false},{"pending",true},{"ordered",true},{"action_uuid",action_id}};
}
}
int main()try{
    using namespace fixture;std::string error;
    for(bool host:{true,false})for(int kind=0;kind<6;++kind){
        reset(host,kind==1?a::QuestListEdit::Title:kind==3?a::QuestListEdit::Kills:kind>=4?a::QuestListEdit::Rewards:a::QuestListEdit::List,kind==0?-1:kind==2?0:1);
        if(kind==5){auto& intent=current_request.npc_quest_list;intent.defaults=true;intent.xp=intent.gold=0;}
        EngineFixture engine;need(engine.execute(fixture_binding,current_request,error)==a::Outcome::Confirmed&&engine.routed==1,error);
        need(perform(fixture_binding,current_request,nullptr,error)==a::Outcome::Confirmed,error);need(originals==1&&seeds==0,"Ordered original-once/no-seed failed");
        a::Evidence evidence;need(observe(fixture_binding,current_request,evidence,error),error);need(evidence.undo_confirmed,"Undo proof absent");
        json detail;need(read_operation(67,action_id,detail,error)&&detail["canonical_after"]["available"]==true&&
            detail["quest_after"]["all_sha256"]==evidence.quest_list_after.all_sha256&&detail["ledger_before"]==detail["ledger_after"],error);
        need(!read_operation(68,action_id,detail,error)&&!read_operation(67,"missing",detail,error),"Detail returned another operation");
        need(perform(fixture_binding,current_request,nullptr,error)==a::Outcome::Unknown&&originals==1,"Replay executed");
        scope_valid=false;need(!observe(fixture_binding,current_request,evidence,error),"Observation escaped consumed scope");}
    for(int fault_case=0;fault_case<7;++fault_case){reset();if(fault_case==0)consumed=false;if(fault_case==1)scope_valid=false;
        if(fault_case==2)Config::instance().is_host=false;if(fault_case==3)current_request.action_uuid="different";
        if(fault_case==4)session_valid=false;if(fault_case==5)current_request.npc_quest_list.edit=a::QuestListEdit(255);
        a::Evidence authority;const auto* supplied=fault_case==6?&authority:nullptr;
        need(perform(fixture_binding,current_request,supplied,error)==a::Outcome::RejectedNoEffect&&originals==0&&seeds==0,"Rejected admission caused effect");}
    for(auto edit:{a::QuestListEdit::Title,a::QuestListEdit::Kills,a::QuestListEdit::Rewards})for(int effect=0;effect<7;++effect){reset(true,edit,0);change_task=effect==0;change_ledger=effect==1;change_region_rng=effect==2;change_references=effect==3;change_survivor=effect==4;original_returned=effect!=5;change_global_rng=effect==6;
        need(perform(fixture_binding,current_request,nullptr,error)==a::Outcome::Unknown,"Unexpected effect confirmed");need(originals==1&&seeds==0,"Failed effect executed again or seeded");
        need(perform(fixture_binding,current_request,nullptr,error)==a::Outcome::Unknown&&originals==1,"Unknown replay executed");}
    reset();inside_go=true;a::Binding actual;need(capture_binding(snapshot_id,actual,error)&&actual==fixture_binding,error);
    std::optional<a::Context> captured;need(capture_context(snapshot_id,fixture_binding,captured,error)&&captured==fixture_context,error);
    pending=Pending{fixture_binding,snapshot_id,owner_key,"",current_request.npc_quest_list,captured};inside_go=false;fixture_controller["pending"]=false;consumed=false;tick();need(submits==1,"Held capture did not submit once");
    reset();captured=fixture_context;pending=Pending{fixture_binding,snapshot_id,owner_key,"",current_request.npc_quest_list,captured};fixture_context.session=6;fixture_controller["pending"]=false;consumed=false;tick();need(submits==0&&fault,"Stale capture submitted");
    reset();std::string one,two;auto native=world();need(canonical_digest(native,one,error),error);
    native.owners.at(5000).state.local[0].task=q::TaskStatus::FinishedWaitingTick;need(canonical_digest(native,two,error)&&one!=two,"Canonical evidence omitted task phase");
    const auto diagnostic=canonical_diagnostic(native);need(diagnostic["available"]==true&&diagnostic["rows"].size()==4&&
        diagnostic["owners"][owner_key][0]["task"]==int(q::TaskStatus::FinishedWaitingTick),"Local boundary diagnostic omitted captured record/task");
    native=world();native.owners.at(5000).state.local[0].manager_member=false;need(canonical_digest(native,two,error)&&one!=two,"Canonical evidence omitted membership");
    native=world();auto relocated=native.owners.extract(5000);relocated.key()=15000;relocated.mapped().pointer=15000;relocated.mapped().uid=999;native.owners.insert(std::move(relocated));native.manager=888;
    need(canonical_digest(native,two,error)&&one==two,"Canonical evidence contains native address/UID");
    need(a::tick_commands::requires_evidence({5,snapshot_id,current_request}),"Quest action lacks peer evidence comparison");
    for(auto edit:{a::QuestListEdit::Kills,a::QuestListEdit::Rewards}){
        current_request.npc_quest_list.edit=edit;need(a::tick_commands::requires_evidence({5,snapshot_id,current_request}),"Quest edit omitted mandatory canonical evidence");
        native=world();if(edit==a::QuestListEdit::Kills)++native.rows[0].record.monsters_to_kill;else ++native.rows[0].record.gold_reward;
        need(canonical_digest(native,two,error)&&one!=two,"Canonical evidence omitted kills/reward field");}
    current_request.npc_quest_list.edit=a::QuestListEdit(255);need(!a::tick_commands::requires_evidence({5,snapshot_id,current_request}),"Unsupported Quest edit entered ordered family");
    json references;std::string targets,digest;native=world();
    need(authoring_effect(fixture_binding,snapshot_id,67,native,references,targets,digest,error),error);
    fixture_references.rows.push_back({{"subscriber_id",7},{"gold",40}});
    need(authoring_effect(fixture_binding,snapshot_id,67,native,references,targets,two,error)&&digest!=two,"Destination digest omitted active references");
    fixture_references.rows.clear();fixture_target_pool="new pool";
    need(authoring_effect(fixture_binding,snapshot_id,67,native,references,targets,two,error)&&digest!=two,"Destination digest omitted target pool");
    fixture_target_pool="pool";native.owners.at(5000).state.local[0].task=q::TaskStatus::Pending;
    need(authoring_effect(fixture_binding,snapshot_id,67,native,references,targets,two,error)&&digest!=two,"Destination digest omitted actual recalculation task");
    EngineFixture engine;need(engine.execute(fixture_binding,current_request,error)==a::Outcome::RejectedNoEffect&&engine.routed==0,"Unsupported Quest reached native engine");
    current_request.action=a::Action::SetNpcQuestDestination;current_request.npc_quest.region=current_request.npc_quest.destination_region=67;
    need(engine.execute(fixture_binding,current_request,error)==a::Outcome::Confirmed&&engine.routed==1,"Same-Region Building destination omitted from consumed engine route");
    current_request.npc_quest.family=a::QuestDestinationFamily::Zone;
    need(engine.execute(fixture_binding,current_request,error)==a::Outcome::RejectedNoEffect&&engine.routed==1,"Unimplemented destination subtype entered ordered engine");
    std::cout<<"Ordered Quest Add/Title/Kills/Rewards/Delete production bodies passed\n";
    return 0;
}catch(const std::exception& x){std::cerr<<x.what()<<'\n';return 1;}
