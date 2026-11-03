#include "typed_action/tick_authority.h"
#include "typed_action/tick_evidence.h"
#include <iostream>
#include <map>
#include <stdexcept>

namespace real=mt2mp::action_session;
namespace tick=real::tick_commands;
namespace fixture_costume {
using json=nlohmann::json;
constexpr const char* fixture_snapshot="22222222-2222-4222-8222-222222222222";
constexpr const char* fixture_id="11111111-1111-4111-8111-111111111111";
const real::Binding fixture_binding{3,4};
unsigned fixture_checks=0,fixture_originals=0,fixture_seeds=0,fixture_submissions=0,fixture_dirty=0,fixture_editor_calls=0;
bool fixture_go=false,fixture_context_fresh=true,fixture_binding_fresh=true,fixture_code_fresh=true;
int fixture_defect=0;uintptr_t fixture_ui_owner=0x200;std::string fixture_ui_draft="Unsaved local draft";
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
struct Window {uintptr_t window=0;};struct Receipt {bool ui_command_returned=false,record_cleanup_confirmed=false;};
struct Calls {bool command(const Api&,const Window&,Receipt&,std::string& error){++fixture_editor_calls;return fail(error,"fixture unrelated Costume Apply entered");}};
Calls fixture_calls;Calls& process_calls(){return fixture_calls;}
template<class Memory>bool resolve_api(Memory&,uintptr_t,const char*,Api&,std::string&){return true;}
}
}
struct Config {bool is_host=true;static Config& instance(){static Config value;return value;}};
struct Memory {};
struct State {uintptr_t image=0x100000;unsigned thread=7;} state;
bool ready=true,fault=false,replaying=false;uintptr_t replay_receiver=0;uint32_t original_calls=0;
uint64_t captures=0,submitted=0;std::string cause,last_action;
constexpr const char* exe_sha="fixture-supported-image";
using Command=a::ui_diagnostic::UiCommand;struct Code {uintptr_t rva=0x6b51c0;}ui_code;
unsigned GetCurrentThreadId(){return 7;}
bool reject(const std::string& error){cause=error;return false;}
bool fixture_original_ui(void*,void*){++fixture_editor_calls;return true;}Command original=&fixture_original_ui;
struct Character {uintptr_t selected=0x200;real::Ledger money;json proof;};
struct Sample {Character character;uintptr_t costume=0x300;std::vector<std::array<uintptr_t,3>> local{{0x200,0x300,0}};real::CostumeEditState value;};
#include "costume_types_actual.inc"
std::optional<Pending> pending;std::map<std::string,Operation> operations;
real::CostumeEditState fixture_live;real::Ledger fixture_money;json fixture_proof,fixture_controller;
struct FixtureRecord {std::string label,category;bool enabled=true;};
namespace net {namespace session_wire {bool connected(){return true;}}std::string current_snapshot_id(){return fixture_snapshot;}}
namespace typed_action_runtime {
json controller_status(){return fixture_controller;}
bool original_go_admission(bool& inside,std::string&){inside=fixture_go;return true;}
bool read_binding(const std::string& snapshot,a::Binding& binding,std::string& error){if(!fixture_binding_fresh||snapshot!=fixture_snapshot)return fail(error,"fixture actual Binding changed");binding=fixture_binding;return true;}
bool network_binding(const std::string& snapshot,a::Binding& binding,std::string& error){if(fixture_go)return fail(error,"fixture network binding called inside OriginalGo");return read_binding(snapshot,binding,error);}
bool submit_costume_edit(const std::string&,const a::CostumeEditIntent&,std::string&){++fixture_submissions;return true;}
}
namespace character_rename {
bool pending_ui(){return false;}bool ordered_mode(){return fixture_controller.value("ordered",false);}
bool same_local(const Character& before,const Character& after){return before.selected==after.selected&&before.money==after.money;}
#include "costume_parent_capture_binding_actual.inc"
}
namespace building_status {bool pending_ui(){return false;}}
namespace dungeon_config {bool pending_ui(){return false;}}
namespace dungeon_sound {bool pending_ui(){return false;}}
namespace dungeon_gizmo {bool pending_ui(){return false;}}
namespace dungeon_palette {bool pending_ui(){return false;}}
namespace dungeon_light {bool pending_ui(){return false;}}
namespace ability_edit {bool pending_ui(){return false;}}
namespace quest_list {bool pending_ui(){return false;}uint32_t label(uintptr_t,void* record,const char* wanted,bool& match){const auto& value=*static_cast<FixtureRecord*>(record);match=value.label==wanted&&value.category.empty();return 0;}}
namespace command_token_label {bool read(Memory&,void* record,const char* wanted,bool& match,std::string&){match=static_cast<FixtureRecord*>(record)->label==wanted;return true;}}
namespace action_runtime {std::string uuid(std::string&){return fixture_id;}uint32_t seed(uintptr_t,const std::array<uint64_t,4>& value){++fixture_seeds;fixture_live.owner.global_random=value;return 0;}}
namespace quest_bridge {bool sha256(const std::string& text,std::string& output,std::string& error){try{output=mt2mp::sha256(std::vector<unsigned char>(text.begin(),text.end()));return true;}catch(const std::exception& why){error=why.what();return false;}}}
#include "costume_record_actual.inc"
bool code(Memory&,std::string& error){return fixture_code_fresh||fail(error,"fixture exact code guard failed");}
bool sample(Memory&,const a::Binding& binding,const std::string& snapshot,const a::CostumeEditIntent&,Sample& output,std::string& error){
    if(!fixture_binding_fresh||binding!=fixture_binding||snapshot!=fixture_snapshot)return fail(error,"fixture current sample Binding failed");output={};output.value=fixture_live;output.character.money=fixture_money;output.character.proof=fixture_proof;
    if(fixture_defect==6&&fixture_originals)++output.costume;return true;}
bool window(Memory&,uintptr_t& outer,uintptr_t& live,uintptr_t& draft,uintptr_t& owner,std::string&){outer=0x100;live=0x300;draft=0x500;owner=fixture_ui_owner;return true;}
uint32_t setup(uintptr_t,uintptr_t,uintptr_t){++fixture_editor_calls;return 1;}
bool prepare(uintptr_t,const a::CostumeRecord&,std::string& error){++fixture_editor_calls;return fail(error,"fixture broader Costume preparation entered");}
bool read_record(Memory&,uintptr_t object,a::CostumeRecord& record,std::string&){record=fixture_live.record;if(object==0x500)record.name=fixture_ui_draft;return true;}
template<class T>bool field(Memory&,uintptr_t object,size_t offset,T& output,std::string& error){
    if(object==0x200&&offset==0x2c){output=T(0);return true;}if(object==0x200&&offset==0x30){output=T(1);return true;}return fail(error,"fixture exact selected Costume owner field failed");}
bool read_weapon_command(Memory&,uintptr_t record,a::CostumeWeaponIntent& intent,std::string&){const auto& value=*reinterpret_cast<FixtureRecord*>(record);intent={value.category,value.enabled};return true;}
uint32_t weapon_dirty(uintptr_t){++fixture_dirty;return 0;}
bool weapon_dirty_readback(Memory&,uintptr_t,std::string&){return true;}
uint32_t weapon_original(uintptr_t owner,const a::CostumeWeaponIntent& intent,uint32_t& calls,bool& returned){
    ++fixture_originals;++calls;if(owner!=0x200||fixture_defect==1)return 1;
    if(fixture_defect!=7){auto& rows=fixture_live.weapon_categories;const auto at=std::find(rows.begin(),rows.end(),intent.category);
        if(intent.enabled){if(at==rows.end())rows.push_back(intent.category);}else if(at!=rows.end()){*at=rows.back();rows.pop_back();}
        fixture_live.all_sha256=std::string(64,'f');fixture_live.owner.all_sha256=std::string(64,'c');}
    if(fixture_defect==2)fixture_live.record.name="Unrelated Costume edit";
    if(fixture_defect==3)fixture_proof["quests_sha256"]=std::string(64,'d');
    if(fixture_defect==4)++fixture_money.cash;
    if(fixture_defect==5)++fixture_live.owner.global_random[0];
    if(fixture_defect==8)++fixture_live.owner.speed;
    returned=true;return 0;
}
#include "costume_helpers_actual.inc"
#include "costume_capture_actual.inc"
#include "costume_tick_actual.inc"
#include "costume_perform_actual.inc"
#include "costume_observe_actual.inc"
struct FixtureSession:real::Session {bool current(real::Context& output,std::string& error)override{return a::fixture_selected.current(output,error);}};
struct FixtureEngine:real::Engine {
    bool bind(const std::string&,real::Binding& output,std::string&)override{output=fixture_binding;return true;}
    bool current(const real::Binding& binding,const std::string& snapshot,std::string& error)override{return (binding==fixture_binding&&snapshot==fixture_snapshot)||fail(error,"fixture Engine owner changed");}
    bool ledger(const real::Binding&,real::Ledger& output,std::string&)override{output=fixture_money;return true;}
    real::Outcome execute_remove(const real::Binding&,const real::Request&,std::string&)override{return real::Outcome::RejectedNoEffect;}
    real::Outcome apply_remove(const real::Binding&,const real::Request&,const real::Evidence&,std::string&)override{return real::Outcome::RejectedNoEffect;}
    bool observe_remove(const real::Binding&,const real::Request&,real::Evidence&,std::string&)override{return false;}
    bool retire(const real::Binding&,const real::Request&,std::string&)override{return true;}
};
void fixture_reset(bool host=true){
    ready=true;fault=replaying=false;pending.reset();operations.clear();cause.clear();fixture_originals=fixture_seeds=fixture_submissions=fixture_dirty=fixture_editor_calls=0;captures=submitted=0;fixture_defect=0;
    fixture_go=false;fixture_context_fresh=fixture_binding_fresh=fixture_code_fresh=true;fixture_ui_owner=0x200;fixture_ui_draft="Unsaved local draft";
    fixture_live={};fixture_live.owner.day=28;fixture_live.owner.rename_day=20;fixture_live.owner.count=13;fixture_live.owner.name=fixture_live.owner.definition_name="Existing class";
    fixture_live.owner.all_sha256=std::string(64,'a');fixture_live.owner.others_sha256=std::string(64,'b');fixture_live.owner.released=true;fixture_live.owner.undo_count=2;fixture_live.owner.global_random={11,12,13,14};fixture_live.owner.release_edits={1,2,3,4,5,6,7,8};
    fixture_live.record.name="Live costume";fixture_live.record.skeleton="humanoid";fixture_live.record.parts.push_back({"hand","model",{},1,{},{{1,1,1,1}}});fixture_live.weapon_categories={"Sword","Staff","Bow"};fixture_live.all_sha256=std::string(64,'e');fixture_live.count=13;
    fixture_money={};fixture_money.record_present=1;fixture_money.cash=fixture_money.record_cash=1000;
    fixture_proof=json::object();for(const auto* key:{"manager_sha256","family_sha256","actors_sha256","bosses_sha256","release_sha256","other_definitions_sha256","other_actors_sha256","quests_sha256","art_store_sha256"})fixture_proof[key]=std::string(64,'a');
    a::fixture_selected.value={77,host?1u:2u,1,3,host?real::Role::Host:real::Role::Client,fixture_snapshot};Config::instance().is_host=host;
    fixture_controller={{"started",true},{"locked",false},{"pending",false},{"ordered",true},{"action_uuid",fixture_id}};
}
real::Request fixture_request(const std::string& category,bool enabled){
    real::Request request;request.action=real::Action::EditCharacterCostume;request.peer=2;request.sequence=1;request.action_uuid=fixture_id;request.snapshot_uuid=fixture_snapshot;
    auto& intent=request.costume_edit;intent.kind=0;intent.id=1;intent.weapon_category=real::CostumeWeaponIntent{category,enabled};intent.record=fixture_live.record;
    std::string error;fixture_need(quest_bridge::sha256(record_json(fixture_live.record).dump(),intent.before_sha256,error),error);request.canonical_record_key=real::costume_key(intent);return request;
}
std::string fixture_consumed(bool host_role,const std::string& category,bool enabled,int defect=0,tick::Receipt* completed=nullptr){
    fixture_reset(host_role);auto request=fixture_request(category,enabled);fixture_defect=defect;fixture_controller["pending"]=true;fixture_ui_owner=0x999;fixture_ui_draft="Another current draft";
    tick::Config hc;hc.context=a::fixture_selected.value;hc.context.local_peer=1;hc.context.role=real::Role::Host;tick::Config cc;cc.context=a::fixture_selected.value;
    auto validate=[](const tick::Input&,std::string&){return true;};tick::Queue host(hc,validate),client(cc,validate);tick::Input input{77,fixture_snapshot,request};tick::Assigned assigned;tick::Batch batch;tick::Receipt receipt;std::string error,digest;
    fixture_need(host.offer(2,input,assigned,error)==tick::Admission::Accepted&&host.seal(batch,error),error);if(!host_role)fixture_need(client.admit(1,batch,error)==tick::Admission::Accepted,error);
    auto& queue=host_role?host:client;FixtureSession session;FixtureEngine engine;
    const bool consumed=queue.consume(1,[](std::string&){return true;},[&](const tick::Assigned& command,std::string& why){return tick::authority::run(queue,session,engine,fixture_binding,command,[&](std::string& cause_value){
        const auto& actual=std::get<real::Request>(command.input.command);auto wrong=actual;wrong.sequence++;
        fixture_need(perform(fixture_binding,wrong,nullptr,cause_value)==a::Outcome::RejectedNoEffect&&!fixture_originals,"Different consumed Costume command reached original");
        auto other_binding=fixture_binding;++other_binding.native_generation;fixture_need(perform(other_binding,actual,nullptr,cause_value)==a::Outcome::RejectedNoEffect&&!fixture_originals,"Different native Costume generation reached original");
        Config::instance().is_host=!host_role;fixture_need(perform(fixture_binding,actual,nullptr,cause_value)==a::Outcome::RejectedNoEffect&&!fixture_originals,"Different actual Costume role reached original");Config::instance().is_host=host_role;
        a::Evidence adopted;fixture_need(perform(fixture_binding,actual,&adopted,cause_value)==a::Outcome::RejectedNoEffect&&!fixture_originals,"Ordered Costume accepted Host evidence");
        if(host_role&&enabled&&!defect){const auto protected_before=fixture_proof;for(const auto* field:{"actors_sha256","bosses_sha256","release_sha256","other_definitions_sha256","other_actors_sha256","quests_sha256","art_store_sha256"}){
            fixture_proof.erase(field);fixture_need(perform(fixture_binding,actual,nullptr,cause_value)==a::Outcome::RejectedNoEffect&&!fixture_originals,"Missing complete protected Costume proof reached original");fixture_proof=protected_before;fixture_proof[field]="bad";
            fixture_need(perform(fixture_binding,actual,nullptr,cause_value)==a::Outcome::RejectedNoEffect&&!fixture_originals,"Invalid protected Costume digest reached original");fixture_proof=protected_before;}}
        const auto outcome=perform(fixture_binding,actual,nullptr,cause_value);fixture_need(!fixture_seeds&&!fixture_editor_calls&&!fixture_dirty&&fixture_ui_owner==0x999&&fixture_ui_draft=="Another current draft","Ordered Costume adopted RNG or touched incidental editor");
        if(defect){fixture_need(outcome==a::Outcome::Unknown,"Partial/unrelated Costume effect confirmed: "+cause_value);const auto calls=fixture_originals;fixture_need(perform(fixture_binding,actual,nullptr,cause_value)==a::Outcome::Unknown&&fixture_originals==calls,"Unknown Costume effect replayed");return outcome;}
        fixture_need(outcome==a::Outcome::Confirmed&&fixture_originals==1,cause_value);a::Evidence evidence;
        fixture_need(observe(fixture_binding,actual,evidence,cause_value)&&retire(fixture_binding,actual,cause_value)&&observe(fixture_binding,actual,evidence,cause_value),cause_value);
        tick::ExecutionReceipt executed;executed.command=command;executed.outcome=outcome;executed.evidence=std::make_shared<a::Evidence>(evidence);fixture_need(tick::canonical_evidence(a::fixture_selected.value,command,executed,digest,cause_value),cause_value);fixture_need(queue.bind_evidence(command,digest,cause_value),cause_value);return outcome;
    },why);},[](uint64_t,std::string&){return a::Outcome::Confirmed;},receipt,error);
    fixture_need(consumed==!defect,"Queue outcome differs from actual Costume result: "+error);fixture_need(!tick::authority::active(),"Costume consumed authority escaped command");if(completed)*completed=receipt;return digest;
}
void fixture_cohort(const std::string& category,bool enabled,const std::string& digest,const tick::Receipt& independent_client){
    fixture_reset();auto request=fixture_request(category,enabled);tick::Config config;config.context=a::fixture_selected.value;auto validate=[](const tick::Input&,std::string&){return true;};
    for(int mode=0;mode<3;++mode){tick::Queue queue(config,validate);tick::Input input{77,fixture_snapshot,request};tick::Assigned assigned;tick::Batch batch;tick::Receipt receipt;std::string error;
        fixture_need(queue.offer(2,input,assigned,error)==tick::Admission::Accepted&&queue.seal(batch,error),error);
        const bool consumed=queue.consume(1,[](std::string&){return true;},[&](const tick::Assigned& command,std::string& why){if(mode&&!queue.bind_evidence(command,digest,why))return a::Outcome::Unknown;return a::Outcome::Confirmed;},[](uint64_t,std::string&){return a::Outcome::Confirmed;},receipt,error);
        if(!mode){fixture_need(!consumed&&receipt.outcome==a::Outcome::Unknown,"Missing Costume evidence retired command");continue;}
        fixture_need(consumed&&queue.state().evidence_ack_mask!=3,"Costume retired before full cohort ACK");auto peer=independent_client;if(mode==2)peer.command_evidence.front().sha256=std::string(64,'0');
        if(mode==1)fixture_need(queue.acknowledge(2,peer,error)&&queue.state().evidence_ack_mask==3,"Independent equal Costume proof did not ACK: "+error);else fixture_need(!queue.acknowledge(2,peer,error)&&queue.state().locked,"Different Costume evidence reached ACK");}
}
void fixture_run(){
    fixture_reset();std::string error;fixture_go=true;FixtureRecord record{"WeaponCategory","Axe",true};
    fixture_need(entry(reinterpret_cast<void*>(0x100),&record)&&pending&&pending->context&&captures==1&&!fixture_originals&&fixture_dirty==1,cause);
    fixture_go=false;fixture_ui_owner=0x999;fixture_ui_draft="Changed after capture";tick();fixture_need(fixture_submissions==1&&!pending&&!fixture_originals,"Held Costume capture reread transient editor or lost Context");tick();fixture_need(fixture_submissions==1,"Costume captured intent submitted twice");
    for(int drift=0;drift<3;++drift){fixture_reset();fixture_go=true;fixture_need(entry(reinterpret_cast<void*>(0x100),&record),cause);fixture_go=false;if(drift==0)++a::fixture_selected.value.session;if(drift==1)fixture_binding_fresh=false;if(drift==2)fixture_controller["ordered"]=false;tick();fixture_need(!fixture_submissions&&!pending&&!fixture_originals,"Stale Costume Context/Binding/mode submitted");}
    fixture_reset();auto request=fixture_request("Axe",true);fixture_controller["pending"]=true;fixture_need(perform(fixture_binding,request,nullptr,error)==a::Outcome::RejectedNoEffect&&!fixture_originals,"Unconsumed Costume reached original");
    for(const auto& category:std::vector<std::string>{"Sword","Staff","Bow","Axe","UnlistedGenericCategory"}){const bool enabled=category=="Axe"||category=="UnlistedGenericCategory";tick::Receipt peer;const auto host=fixture_consumed(true,category,enabled),client=fixture_consumed(false,category,enabled,0,&peer);fixture_need(!host.empty()&&host==client,"Independent Costume canonical effect differs");fixture_cohort(category,enabled,host,peer);}
    for(int defect=1;defect<=8;++defect){fixture_consumed(true,"Axe",true,defect);fixture_consumed(false,"Axe",true,defect);}
    fixture_reset();auto visual=fixture_request("Axe",true);visual.costume_edit.weapon_category.reset();tick::Input input{77,fixture_snapshot,visual};fixture_need(!tick::requires_evidence(input),"Broader visual Costume Apply admitted into checkbox family");
}
}
int main()try{fixture_costume::fixture_run();std::cout<<fixture_costume::fixture_checks<<" ordered original Costume weapon capture/Context/consumed/effect/canonical checks passed\n";return 0;}catch(const std::exception& why){std::cerr<<why.what()<<'\n';return 1;}
