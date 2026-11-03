#include "typed_action/tick_authority.h"
#include "typed_action/tick_evidence.h"
#include "ability_adjustment_view.h"
#include "ability_apply_lease.h"
#include <cstring>
#include <iostream>
#include <map>
#include <stdexcept>
#include <windows.h>

namespace real=mt2mp::action_session;
namespace tick=real::tick_commands;
namespace fixture_ability {
using json=nlohmann::json;
namespace ability_edit=::fixture_ability;
namespace ability_apply_lease=mt2mp::game::ability_apply_lease;
namespace ability_adjustment_view=mt2mp::game::ability_adjustment_view;
constexpr const char* fixture_snapshot="22222222-2222-4222-8222-222222222222";
constexpr const char* fixture_id="11111111-1111-4111-8111-111111111111";
const real::Binding fixture_binding{3,4};
unsigned fixture_checks=0,fixture_originals=0,fixture_seeds=0,fixture_submissions=0,fixture_ui_originals=0;
bool fixture_go=false,fixture_context_fresh=true,fixture_binding_fresh=true,fixture_code_fresh=true,fixture_route_fresh=true,fixture_proposal_fresh=true;
int fixture_defect=0;
bool fail(std::string& error,const std::string& why){error=why;return false;}
void fixture_need(bool value,const std::string& why){++fixture_checks;if(!value)throw std::runtime_error(why);}
namespace a {
using namespace real;
struct Selected {Context value;bool current(Context& output,std::string& error){if(!fixture_context_fresh)return fail(error,"fixture actual Session changed");output=value;return true;}};
Selected fixture_selected;Selected& selected_session(){return fixture_selected;}
namespace tick_commands::authority {
bool active(){return tick::authority::active();}
bool matches(const Context& context,const Binding& binding,const Request& request,std::string& error){return tick::authority::matches(context,binding,request,error);}
}
namespace ui_diagnostic {
using UiCommand=bool(*)(void*,void*);
struct Api {uintptr_t label_equals=1;UiCommand command=nullptr;const char* destroy_label=nullptr;};
struct Window {uintptr_t window=0;};
struct Receipt {bool ui_command_returned=false,record_cleanup_confirmed=false;};
struct Calls {bool command(const Api&,const Window&,Receipt&,std::string& error);};
Calls fixture_calls;Calls& process_calls(){return fixture_calls;}
template<class Memory>bool resolve_api(Memory&,uintptr_t,const char*,Api&,std::string&){return true;}
}
}
struct Config {bool is_host=true;static Config& instance(){static Config value;return value;}};
struct Memory {bool copy(uintptr_t,void*,size_t,std::string&);};
struct State {uintptr_t image=0x100000;unsigned thread=7;} state;
bool ready=true,fault=false,replaying=false;
uintptr_t replay_receiver=0;uint32_t original_calls=0;
uint64_t captures=0,submitted=0;std::string cause,last_action;
constexpr const char* exe_sha="fixture-supported-image";
using Command=a::ui_diagnostic::UiCommand;
struct Code {uintptr_t rva=0x5714c0;}ui_code;
unsigned GetCurrentThreadId(){return 7;}
bool reject(const std::string& error){cause=error;return false;}
struct Character {uintptr_t selected=0x200;real::Ledger money;json proof;};
struct Sample {Character character;uintptr_t prototype=0x300,proxy=0x400;uint64_t uid=91;std::vector<std::array<uintptr_t,3>> local{{0x300,91,0x400}};real::AbilityEditState value;};
uintptr_t fixture_live_proxy=0x400;
#include "ability_types_actual.inc"
std::optional<Pending> pending;
std::map<std::string,Operation> operations;
std::map<std::string,uintptr_t> adjustment_views;
struct ViewTail {bool armed=false,preserve=false;uintptr_t view=0;uint32_t calls=0;};ViewTail view_tail;
real::AbilityEditState fixture_live;
real::Ledger fixture_money;
json fixture_proof,fixture_controller;
ability_adjustment_view::State fixture_proposal{0x100,0x200,0x300,0,0,1};
namespace net {namespace session_wire {bool connected(){return true;}}std::string current_snapshot_id(){return fixture_snapshot;}}
namespace typed_action_runtime {
json controller_status(){return fixture_controller;}
bool original_go_admission(bool& inside,std::string&){inside=fixture_go;return true;}
bool read_binding(const std::string& snapshot,a::Binding& binding,std::string& error){
    if(!fixture_binding_fresh||snapshot!=fixture_snapshot)return fail(error,"fixture actual Binding changed");binding=fixture_binding;return true;}
bool network_binding(const std::string& snapshot,a::Binding& binding,std::string& error){
    if(fixture_go)return fail(error,"fixture held submission called inside OriginalGo");return read_binding(snapshot,binding,error);}
bool submit_ability_edit(const std::string&,const a::AbilityEditIntent&,std::string&){++fixture_submissions;return true;}
}
namespace character_rename {
unsigned diagnostic_submissions=0;
std::string native_edit_input_token(){return "capture-token";}
bool native_edit_submitted(const std::string& token,const std::string& id,std::string& error){
    ++diagnostic_submissions;return (token=="capture-token"&&id==fixture_id)||fail(error,"fixture diagnostic UUID link mismatch");}
bool pending_ui(){return false;}
bool ordered_mode(){return fixture_controller.value("ordered",false);}
bool presentation(void*,void*,uintptr_t,bool& handled,std::string&){handled=false;return true;}
bool preview_presentation(void*,void*,void(*)(void*,void*),bool& handled,std::string&){handled=false;return true;}
bool same_local(const Character& before,const Character& after){return before.selected==after.selected&&before.money==after.money;}
bool window(Memory&,uintptr_t,uintptr_t& view,uintptr_t& editor,uintptr_t& outer,std::string& error){
    if(!fixture_proposal_fresh)return fail(error,"fixture local proposal View unavailable");view=0x100;editor=0x500;outer=0x600;return true;}
#include "ability_parent_capture_binding_actual.inc"
}
namespace character_membership {bool import_presentation(void*,void*,uintptr_t,bool& handled,std::string&){handled=false;return true;}}
namespace costume_edit {bool pending_ui(){return false;}}
namespace quest_list {uint32_t label(uintptr_t,void* record,const char* wanted,bool& match){match=std::string(static_cast<const char*>(record))==wanted;return 0;}}
void* _ReturnAddress(){return reinterpret_cast<void*>(state.image+(fixture_defect==6?0:0x570b63));}
void fixture_view_original(void*,void*){++fixture_ui_originals;}
void(*original_view_select)(void*,void*)=&fixture_view_original;
namespace action_runtime {
std::string uuid(std::string&){return fixture_id;}
uint32_t seed(uintptr_t,const std::array<uint64_t,4>& value){++fixture_seeds;fixture_live.owner.global_random=value;return 0;}
}
namespace quest_bridge {bool sha256(const std::string& text,std::string& output,std::string& error){
    try{output=mt2mp::sha256(std::vector<unsigned char>(text.begin(),text.end()));return true;}catch(const std::exception& why){error=why.what();return false;}}
}
#include "ability_record_actual.inc"
bool code(Memory&,std::string& error){return fixture_code_fresh||fail(error,"fixture exact original code guard failed");}
bool view_route(Memory&,std::string& error){return fixture_route_fresh||fail(error,"fixture exact original presentation route failed");}
bool read_proposal(Memory&,uintptr_t view,ability_adjustment_view::State& output,std::string& error){
    if(!fixture_proposal_fresh||view!=0x100)return fail(error,"fixture actual local proposal read failed");output=fixture_proposal;return true;}
uint32_t write_proposal(const ability_adjustment_view::State& value){
    fixture_proposal.prototype=value.prototype;fixture_proposal.cost=value.cost;fixture_proposal.index=value.index;fixture_proposal.delta=value.delta;return fixture_defect==8?1u:0u;}
bool sample(Memory&,const a::Binding& binding,const std::string& snapshot,const a::AbilityEditIntent&,Sample& output,std::string& error){
    if(!fixture_binding_fresh||binding!=fixture_binding||snapshot!=fixture_snapshot)return fail(error,"fixture exact current sample binding failed");
    output={};output.value=fixture_live;output.character.money=fixture_money;output.character.proof=fixture_proof;
    output.proxy=fixture_live_proxy;output.local[0][2]=fixture_live_proxy;
    if(fixture_defect==9)++output.uid;return true;
}
ability_apply_lease::Slots fixture_slots;
std::array<unsigned char,0x608> fixture_outer{};
a::AbilityRecord fixture_local_draft,fixture_clone_record;
unsigned fixture_destroyed=0,fixture_released=0,fixture_forwarded=0;
int fixture_refs=0;bool fixture_clone_alive=false;
std::string fixture_widget_buffer="unsent editor text";int fixture_selection=17;
bool Memory::copy(uintptr_t address,void* out,size_t size,std::string& error){
    if(address!=0x600||size!=fixture_outer.size())return fail(error,"fixture outer object read mismatch");
    std::memcpy(out,fixture_outer.data(),size);return true;
}
bool window(Memory&,uintptr_t& outer,uintptr_t& live,uintptr_t& draft,uintptr_t& owner,std::string&){outer=fixture_slots.outer;live=fixture_slots.live;draft=fixture_slots.draft;owner=fixture_slots.owner;return true;}
uint32_t setup(uintptr_t,uintptr_t,uintptr_t){++fixture_ui_originals;return 0;}
uint32_t prepare(uintptr_t,const a::AbilityRecord& record){fixture_clone_record=record;return 0;}
uint32_t simplify(uintptr_t){return 0;}
bool read_record(Memory&,uintptr_t object,a::AbilityRecord& record,std::string&){record=object==0x9000?fixture_clone_record:(object==0x500?fixture_local_draft:fixture_live.record);return true;}
template<class T>bool field(Memory&,uintptr_t object,size_t offset,T& output,std::string& error){
    if(object==0x200&&offset==0x2c){output=T(0);return true;}if(object==0x200&&offset==0x30){output=T(1);return true;}
    if(object==0x200&&offset==0x130){output=T(0x700);return true;}if(object==0x200&&offset==0x138){output=T(1);return true;}
    if(object==0x700&&offset==0){output=T(0x300);return true;}
    if(object>0x1000000){std::memcpy(&output,reinterpret_cast<const void*>(object+offset),sizeof output);return true;}
    return fail(error,"fixture selected field read mismatch");
}
#include "ability_same_local_actual.inc"
#include "ability_ordered_helpers_actual.inc"
#include "ability_view_tail_actual.inc"
uint32_t invoke_adjustment(uintptr_t view,bool& returned){
    ++fixture_originals;if(fixture_defect==1){returned=false;return 1;}
    if(view!=0x100||fixture_proposal.prototype!=0x300)return 2;
    auto& rows=fixture_proposal.cost?fixture_live.record.costs:fixture_live.record.effects;
    if(fixture_proposal.index<0||size_t(fixture_proposal.index)>=rows.size())return 3;
    const auto value=rows[size_t(fixture_proposal.index)].values[0]+fixture_proposal.delta;
    if(value==0)rows.erase(rows.begin()+fixture_proposal.index);else rows[size_t(fixture_proposal.index)].values[0]=value;
    fixture_live.all_sha256=std::string(64,'f');fixture_live.owner.all_sha256=std::string(64,'c');
    if(fixture_defect==2)++fixture_live.record.level;
    if(fixture_defect==3)fixture_proof["quests_sha256"]=std::string(64,'d');
    if(fixture_defect==4)++fixture_money.cash;
    if(fixture_defect==5)++fixture_live.owner.global_random[0];
    view_select_entry(reinterpret_cast<void*>(fixture_defect==6?0x101:view),reinterpret_cast<void*>(fixture_proposal.selected));
    if(fixture_defect==7)view_select_entry(reinterpret_cast<void*>(view),reinterpret_cast<void*>(fixture_proposal.selected));
    returned=true;return 0;
}
#include "ability_adjustment_calls_actual.inc"
#include "ability_capture_actual.inc"
#include "ability_tick_actual.inc"
#include "ability_adjustment_perform_actual.inc"
#include "ability_apply_tails_actual.inc"
bool proposal_release(uintptr_t,std::string&){return true;}
bool apply_route(Memory&,std::string& error){return fixture_route_fresh||fail(error,"fixture Apply route changed");}
bool apply_slots(Memory&,uintptr_t outer,ability_apply_lease::Slots& out,std::string& error){if(outer!=0x600)return fail(error,"fixture outer mismatch");out=fixture_slots;return true;}
uint32_t apply_write_slots(const ability_apply_lease::Slots& value){
    fixture_slots=value;
    std::memcpy(fixture_outer.data()+0x510,&value.draft,8);std::memcpy(fixture_outer.data()+0x520,&value.live,8);
    std::memcpy(fixture_outer.data()+0x528,&value.proxy,8);std::memcpy(fixture_outer.data()+0x530,&value.owner,8);std::memcpy(fixture_outer.data()+0x606,value.dirty.data(),2);
    if(fixture_defect==8&&value.draft==0x500){fixture_slots.draft=0x9000;return 8;}
    if(fixture_defect==12&&value.draft==0x9000)return 12;
    return 0;
}
uint32_t apply_clone(uintptr_t source,uintptr_t& output,bool& constructed){
    if(fixture_defect==13){output=0x9000;fixture_clone_alive=true;return 13;}
    if(source!=0x300)return 1;output=0x9000;constructed=true;fixture_clone_alive=true;fixture_clone_record=fixture_live.record;return 0;
}
uint32_t apply_acquire_proxy(uintptr_t,uintptr_t& proxy,bool& acquired){proxy=fixture_live_proxy=0x400;++fixture_refs;acquired=true;return 0;}
uint32_t apply_destroy(uintptr_t draft,uintptr_t live,uint64_t uid){
    if(live!=0x300||uid!=91)return ERROR_INVALID_DATA;
    ++fixture_destroyed;if(draft!=0x9000||!fixture_clone_alive||fixture_slots.draft==draft)return 1;
    if(fixture_defect==10)return 10;
    fixture_clone_alive=false;if(fixture_defect==11)fixture_live.owner.others_sha256=std::string(64,'9');return 0;
}
uint32_t apply_release(uintptr_t){++fixture_released;--fixture_refs;return fixture_defect==9?9u:0u;}
void fixture_setup_original(void*,void*){++fixture_ui_originals;}
void fixture_event_original(void*,const void*){++fixture_forwarded;}
bool a::ui_diagnostic::Calls::command(const Api&,const Window& target,Receipt& receipt,std::string& error){
    ++fixture_originals;++original_calls;
    if(fixture_defect==14)throw std::runtime_error("fixture original exception");
    if(fixture_defect==1)return fail(error,"fixture original command failed");
    if(!replaying||target.window!=fixture_slots.outer||fixture_slots.draft!=0x9000||fixture_slots.live!=0x300||fixture_slots.owner!=0x200)
        return fail(error,"fixture native borrowed bindings differ");
    fixture_live.record=fixture_clone_record;fixture_live.owner.release_edits[7]+=fixture_live.owner.edited?1:2;
    fixture_live.owner.edited=true;fixture_live.owner.rename_day=fixture_live.owner.day;fixture_live.owner.all_sha256=std::string(64,'c');fixture_live.all_sha256=std::string(64,'f');
    fixture_proof["actors_sha256"]=std::string(64,'f');fixture_proof["release_sha256"]=std::string(64,'f');
    fixture_outer[0x606]=1;fixture_slots.dirty[0]=1;
    apply_setup_entry(reinterpret_cast<void*>(target.window),reinterpret_cast<void*>(0x300));
    std::array<unsigned char,0xb8> event{};int32_t type=40;uintptr_t live=0x300;
    std::memcpy(event.data(),&type,4);std::memcpy(event.data()+0xb0,&live,8);
    apply_event_entry(reinterpret_cast<void*>(target.window),event.data());
    apply_event_entry(reinterpret_cast<void*>(0x777),event.data());
    if(fixture_defect==7)apply_event_entry(reinterpret_cast<void*>(target.window),event.data());
    if(fixture_defect==2)++fixture_live.record.level;
    if(fixture_defect==3)fixture_live.owner.others_sha256=std::string(64,'d');
    if(fixture_defect==4)++fixture_money.cash;
    if(fixture_defect==5)++fixture_outer[0x580];
    receipt.ui_command_returned=receipt.record_cleanup_confirmed=true;return true;
}
#include "ability_apply_body_actual.inc"
#include "ability_perform_actual.inc"
#include "ability_observe_actual.inc"

namespace dungeon_config {bool pending_ui(){return false;}}
namespace dungeon_sound {bool pending_ui(){return false;}}
namespace dungeon_gizmo {bool pending_ui(){return false;}}
namespace dungeon_palette {bool pending_ui(){return false;}}
namespace dungeon_light {bool pending_ui(){return false;}}
namespace building_status {bool pending_ui(){return false;}}
namespace quest_list {bool pending_ui(){return false;}}
unsigned fixture_queries=0;
bool fixture_capture_original(void*,void*){++fixture_queries;return true;}
Command original=&fixture_capture_original;
#include "ability_apply_capture_actual.inc"
struct FixtureSession:real::Session {bool current(real::Context& output,std::string& error)override{return a::fixture_selected.current(output,error);}};
struct FixtureEngine:real::Engine {
    bool fixture_fresh=true;
    bool bind(const std::string&,real::Binding& output,std::string&)override{output=fixture_binding;return true;}
    bool current(const real::Binding& binding,const std::string& snapshot,std::string& error)override{return (fixture_fresh&&binding==fixture_binding&&snapshot==fixture_snapshot)||fail(error,"fixture Engine owner changed");}
    bool ledger(const real::Binding&,real::Ledger& output,std::string&)override{output=fixture_money;return true;}
    real::Outcome execute_remove(const real::Binding&,const real::Request&,std::string&)override{return real::Outcome::RejectedNoEffect;}
    real::Outcome apply_remove(const real::Binding&,const real::Request&,const real::Evidence&,std::string&)override{return real::Outcome::RejectedNoEffect;}
    bool observe_remove(const real::Binding&,const real::Request&,real::Evidence&,std::string&)override{return false;}
    bool retire(const real::Binding&,const real::Request&,std::string&)override{return true;}
};
void fixture_reset(bool host=true){
    ready=true;fault=replaying=false;pending.reset();operations.clear();adjustment_views.clear();view_tail={};cause.clear();
    fixture_originals=fixture_seeds=fixture_submissions=fixture_ui_originals=0;captures=submitted=0;fixture_defect=0;
    fixture_go=false;fixture_context_fresh=fixture_binding_fresh=fixture_code_fresh=fixture_route_fresh=fixture_proposal_fresh=true;
    fixture_live={};fixture_live.owner.day=28;fixture_live.owner.rename_day=20;fixture_live.owner.count=13;
    fixture_live.owner.name=fixture_live.owner.definition_name="Existing class";fixture_live.owner.all_sha256=std::string(64,'a');fixture_live.owner.others_sha256=std::string(64,'b');
    fixture_live.owner.released=true;fixture_live.owner.undo_count=2;fixture_live.owner.global_random={11,12,13,14};fixture_live.owner.release_edits={1,2,3,4,5,6,7,8};
    auto& record=fixture_live.record;record.text={"Strike","icon","weapon","animation","path","cast","effect","description"};record.item_value=2;record.item_kind=1;
    record.selectors={1,2,3,4,5};record.color={.1f,.2f,.3f,1};record.timing={0,1,2};record.final_values={1,2};record.level=1;
    record.effects.push_back({{1,0,0},{3,4},5});record.costs.push_back({{2,0,1},{-4,5},6});fixture_live.all_sha256=std::string(64,'e');fixture_live.count=22;
    fixture_money={};fixture_money.record_present=1;fixture_money.cash=fixture_money.record_cash=1000;
    fixture_proof=json::object();for(const auto* key:{"manager_sha256","family_sha256","actors_sha256","bosses_sha256","release_sha256","other_definitions_sha256","other_actors_sha256","quests_sha256","art_store_sha256"})fixture_proof[key]=std::string(64,'a');
    a::fixture_selected.value={77,host?1u:2u,1,3,host?real::Role::Host:real::Role::Client,fixture_snapshot};Config::instance().is_host=host;
    fixture_controller={{"started",true},{"locked",false},{"pending",false},{"ordered",true},{"action_uuid",fixture_id}};
    fixture_proposal={0x100,0x200,0x300,0,0,1};
    fixture_slots={0x600,0x500,0x888,0x889,0x999,{1,1}};fixture_outer.fill(0);apply_write_slots(fixture_slots);
    fixture_local_draft=fixture_live.record;fixture_local_draft.text[0]="Unsent unrelated ability";fixture_local_draft.animation=19;
    fixture_refs=fixture_destroyed=fixture_released=fixture_forwarded=0;fixture_clone_alive=false;fixture_live_proxy=0x400;
    original_apply_setup=&fixture_setup_original;original_apply_event=&fixture_event_original;apply_tail={};
}
real::Request fixture_request(bool cost=false,bool remove=false){
    real::Request request;request.action=real::Action::EditCharacterAbility;request.peer=2;request.sequence=1;request.action_uuid=fixture_id;request.snapshot_uuid=fixture_snapshot;
    auto& intent=request.ability_edit;intent.kind=0;intent.id=1;intent.slot=0;intent.adjustment=real::AbilityAdjustment{cost,0,remove?(cost?4.f:-3.f):(cost?-1.f:1.f)};
    intent.adjustment.reset();intent.record=fixture_live.record;intent.record.text[0]="Strong strike";intent.record.text[7]="Changed description";intent.record.animation=0x01020304;intent.record.use_weapon=true;
    std::string error;fixture_need(quest_bridge::sha256(record_json(fixture_live.record).dump(),intent.before_sha256,error),error);request.canonical_record_key=real::ability_key(intent);return request;
}

std::string fixture_consumed(bool host_role,int defect=0,bool absent_proxy=false){
    fixture_reset(host_role);if(absent_proxy)fixture_live_proxy=0;auto request=fixture_request();fixture_defect=defect;fixture_controller["pending"]=true;
    const auto saved_slots=fixture_slots;const auto saved_outer=fixture_outer;const auto saved_draft=fixture_local_draft;
    tick::Config config;config.context=a::fixture_selected.value;config.context.role=real::Role::Host;config.context.local_peer=1;
    tick::Queue host(config,[](const tick::Input&,std::string&){return true;});
    tick::Config peer_config;peer_config.context=a::fixture_selected.value;
    tick::Queue client(peer_config,[](const tick::Input&,std::string&){return true;});
    tick::Input input{77,fixture_snapshot,request};tick::Assigned assigned;tick::Batch batch;tick::Receipt receipt;std::string error,digest;
    fixture_need(host.offer(2,input,assigned,error)==tick::Admission::Accepted&&host.seal(batch,error),error);
    if(!host_role)fixture_need(client.admit(1,batch,error)==tick::Admission::Accepted,error);
    auto& queue=host_role?host:client;FixtureSession session;FixtureEngine engine;
    const bool consumed=queue.consume(1,[](std::string&){return true;},[&](const tick::Assigned& command,std::string& why){
        return tick::authority::run(queue,session,engine,fixture_binding,command,[&](std::string& cause_value){
            const auto& actual=std::get<real::Request>(command.input.command);
            a::Evidence host_state;fixture_need(perform(fixture_binding,actual,&host_state,cause_value)==a::Outcome::RejectedNoEffect&&!fixture_originals,"Apply adopted Host evidence");
            const auto outcome=perform(fixture_binding,actual,nullptr,cause_value);
            fixture_need(!fixture_seeds&&!fixture_ui_originals,"Apply seeded RNG or rebuilt local editor");
            fixture_need(fixture_widget_buffer=="unsent editor text"&&fixture_selection==17&&fixture_local_draft==saved_draft,"Apply mutated local unsent widget/draft/selection");
            fixture_need(!apply_tail.armed&&!replaying,"Apply scoped presentation escaped original");
            if(defect){fixture_need(outcome==a::Outcome::Unknown,"Defective Apply confirmed: "+cause_value);
                const auto count=fixture_originals;fixture_need(perform(fixture_binding,actual,nullptr,cause_value)==a::Outcome::Unknown&&count==fixture_originals,"Unknown Apply replayed");
                if(defect!=8)fixture_need(fixture_slots==saved_slots,"Apply failure did not restore slots");
                if(defect==8||defect==13)fixture_need(!fixture_destroyed&&!fixture_released,"Unrestored or incomplete working pointers destroyed");
                else fixture_need(fixture_destroyed==1&&fixture_released==1,"Apply failure did not attempt each owned cleanup");
                return outcome;
            }
            fixture_need(outcome==a::Outcome::Confirmed,cause_value);
            fixture_need(fixture_originals==1&&fixture_forwarded==1&&fixture_slots==saved_slots&&fixture_outer==saved_outer,"Apply once/presentation preservation differs");
            fixture_need(fixture_destroyed==1&&fixture_released==1&&!fixture_clone_alive&&fixture_refs==0,"Apply native cleanup balance differs");
            a::Evidence evidence;fixture_need(observe(fixture_binding,actual,evidence,cause_value)&&retire(fixture_binding,actual,cause_value)&&observe(fixture_binding,actual,evidence,cause_value),cause_value);
            fixture_need(evidence.ability_edit_after.record.animation==0x01020304&&evidence.ability_edit_after.record.use_weapon,"Native animation/weapon record was not retained");
            tick::ExecutionReceipt executed;executed.command=command;executed.outcome=outcome;executed.evidence=std::make_shared<a::Evidence>(evidence);
            fixture_need(tick::canonical_evidence(a::fixture_selected.value,command,executed,digest,cause_value),cause_value);
            fixture_need(queue.bind_evidence(command,digest,cause_value),cause_value);return outcome;
        },why);
    },[](uint64_t,std::string&){return a::Outcome::Confirmed;},receipt,error);
    fixture_need(consumed==!defect,"Apply queue outcome differs: "+error);return digest;
}
void fixture_run(){
    fixture_reset();fixture_slots={0x600,0x500,0x300,0x400,0x200,{1,1}};apply_write_slots(fixture_slots);
    fixture_local_draft=fixture_live.record;fixture_local_draft.text[0]="Captured live text";fixture_local_draft.animation=0x01020304;
    fixture_go=true;fixture_queries=0;character_rename::diagnostic_submissions=0;
    fixture_need(entry(reinterpret_cast<void*>(0x600),const_cast<char*>("Apply"))&&pending&&pending->context&&captures==1&&!fixture_queries,"Actual live Apply capture did not retain Context without original execution");
    fixture_need(pending->intent.record==fixture_local_draft&&pending->diagnostic_token=="capture-token","Actual Apply capture lost full committed draft or diagnostic token");
    fixture_go=false;tick();fixture_need(fixture_submissions==1&&character_rename::diagnostic_submissions==1&&!pending,"Held Apply submission did not link exact UUID");
    tick();fixture_need(fixture_submissions==1,"Apply capture submitted twice");
    fixture_reset();fixture_slots={0x600,0x500,0x300,0x400,0x200,{1,1}};fixture_local_draft=fixture_live.record;fixture_local_draft.effects.clear();fixture_go=true;
    fixture_need(entry(reinterpret_cast<void*>(0x600),const_cast<char*>("Apply"))&&!pending&&fixture_queries==1&&!fixture_originals,"Zero-effect local Apply did not retain ordinary native validation without an ordered action");

    fixture_reset();auto request=fixture_request();std::string error;fixture_controller["pending"]=true;
    fixture_need(perform(fixture_binding,request,nullptr,error)==a::Outcome::RejectedNoEffect&&!fixture_originals,"Unconsumed Apply reached original");
    const auto host=fixture_consumed(true),client=fixture_consumed(false);fixture_need(!host.empty()&&host==client,"Independent Apply canonical digests differ");
    fixture_need(fixture_consumed(true,0,true)==fixture_consumed(false,0,true),"Initially absent native proxy changed canonical receipt");
    for(int defect=1;defect<=14;++defect){fixture_consumed(true,defect);fixture_consumed(false,defect);}
}
}
namespace disposal_fixture {
#include "ability_apply_disposal_actual.inc"
alignas(8) std::array<unsigned char,0x210> live{},draft{},foreign{};
std::map<uint32_t,uintptr_t> registry;
unsigned destroyed=0,children_destroyed=0;bool partial_failure=false;
constexpr uint64_t uid=0x123456780000005bULL;
constexpr uintptr_t ability_class=0x171c930;
uintptr_t address(const auto& object){return reinterpret_cast<uintptr_t>(object.data());}
void put(auto& object,size_t offset,uint64_t value){std::memcpy(object.data()+offset,&value,8);}
uint64_t get(const auto& object,size_t offset){uint64_t value=0;std::memcpy(&value,object.data()+offset,8);return value;}
uintptr_t find(uint64_t value){if(!value)return 0;const auto row=registry.find(uint32_t(value));return row==registry.end()?0:row->second;}
void destroy(void* object){
    ++destroyed;auto* bytes=static_cast<unsigned char*>(object);uint64_t copied_uid=0;std::memcpy(&copied_uid,bytes+0x18,8);
    if(partial_failure)RaiseException(0xe0000001,0,0,nullptr);
    ++children_destroyed;registry.erase(uint32_t(copied_uid));std::memset(bytes,0xdd,0x210);
}
void reset(){
    live={};draft={};foreign={};registry.clear();destroyed=children_destroyed=0;partial_failure=false;
    put(live,0,ability_class);put(draft,0,ability_class);put(live,0x18,uid);put(draft,0x18,uid);
    registry[uint32_t(uid & 0xffffffffULL)]=address(live);
}
uint32_t dispose(){return apply_destroy_owned(address(draft),address(live),uid,ability_class,find,destroy);}
void run(){
    using fixture_ability::fixture_need;
    reset();destroy(draft.data());fixture_need(!find(uid)&&get(live,0x18)==uid,"Pinned key-only destructor model did not reproduce lost live lookup");
    reset();const auto original=live;fixture_need(dispose()==0,"Actual owned disposal failed");
    fixture_need(destroyed==1&&children_destroyed==1&&live==original&&find(uid)==address(live),"Actual disposal changed live UID/lookup or skipped original child destruction");
    for(int defect=0;defect<8;++defect){
        reset();
        if(defect==0)put(draft,0x18,uid+1);
        if(defect==1)put(live,0x18,uid+(1ULL<<32));
        if(defect==2)put(draft,0,ability_class+8);
        if(defect==3)put(live,0,ability_class+8);
        if(defect==4)put(draft,0x40,0x888);
        if(defect==5)registry.clear();
        if(defect==6)registry[uint32_t(uid & 0xffffffffULL)]=address(foreign);
        if(defect==7)registry[0]=address(foreign);
        const auto before=draft;fixture_need(dispose()==ERROR_INVALID_DATA&&!destroyed&&draft==before,"Actual disposal accepted stale identity/proxy or foreign UID owner");
    }
    reset();fixture_need(apply_destroy_owned(address(live),address(live),uid,ability_class,find,destroy)==ERROR_INVALID_DATA&&!destroyed,"Live object accepted as scratch");
    fixture_need(apply_destroy_owned(address(draft),address(live),0,ability_class,find,destroy)==ERROR_INVALID_DATA&&!destroyed,"Zero borrowed identity accepted");
    reset();partial_failure=true;fixture_need(dispose()==0xe0000001&&destroyed==1&&!children_destroyed&&get(draft,0x18)==0&&find(uid)==address(live),"Partial native destruction lost live lookup or concealed failure");
    fixture_need(dispose()==ERROR_INVALID_DATA&&destroyed==1,"Partial destruction retried an identity-less scratch");
}
}
int main()try{fixture_ability::fixture_run();disposal_fixture::run();std::cout<<fixture_ability::fixture_checks<<" actual-body live Apply/lease/presentation/evidence checks passed\n";return 0;}
catch(const std::exception& why){std::cerr<<why.what()<<'\n';return 1;}
