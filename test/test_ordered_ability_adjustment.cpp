#include "typed_action/tick_authority.h"
#include "typed_action/tick_evidence.h"
#include "ability_adjustment_view.h"
#include <iostream>
#include <map>
#include <stdexcept>

namespace real=mt2mp::action_session;
namespace tick=real::tick_commands;
namespace fixture_ability {
using json=nlohmann::json;
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
struct Calls {bool command(const Api&,const Window&,Receipt&,std::string& error){return fail(error,"fixture unrelated editor Apply branch entered");}};
Calls fixture_calls;Calls& process_calls(){return fixture_calls;}
template<class Memory>bool resolve_api(Memory&,uintptr_t,const char*,Api&,std::string&){return true;}
}
}
struct Config {bool is_host=true;static Config& instance(){static Config value;return value;}};
struct Memory {};
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
bool native_edit_submitted(const std::string&,const std::string&,std::string&){return true;}
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
void* _ReturnAddress(){return nullptr;}
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
    if(fixture_defect==9)++output.uid;return true;
}
bool window(Memory&,uintptr_t& outer,uintptr_t& live,uintptr_t& draft,uintptr_t& owner,std::string&){outer=0x600;live=0x300;draft=0x500;owner=0x200;return true;}
uint32_t setup(uintptr_t,uintptr_t,uintptr_t){return 0;}
uint32_t prepare(uintptr_t,const a::AbilityRecord&){return 0;}
uint32_t simplify(uintptr_t){return 0;}
bool read_record(Memory&,uintptr_t,a::AbilityRecord& record,std::string&){record=fixture_live.record;return true;}
template<class T>bool field(Memory&,uintptr_t object,size_t offset,T& output,std::string& error){
    if(object==0x200&&offset==0x2c){output=T(0);return true;}if(object==0x200&&offset==0x30){output=T(1);return true;}
    if(object==0x200&&offset==0x130){output=T(0x700);return true;}if(object==0x200&&offset==0x138){output=T(1);return true;}
    if(object==0x700&&offset==0){output=T(0x300);return true;}return fail(error,"fixture exact selected Ability owner field failed");}
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
a::Outcome perform_apply(Memory&,const a::Binding&,const a::Request&,const Sample&,std::string& error){error="fixture unrelated Apply";return a::Outcome::RejectedNoEffect;}
#include "ability_perform_actual.inc"
#include "ability_observe_actual.inc"

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
}
real::Request fixture_request(bool cost=false,bool remove=false){
    real::Request request;request.action=real::Action::EditCharacterAbility;request.peer=2;request.sequence=1;request.action_uuid=fixture_id;request.snapshot_uuid=fixture_snapshot;
    auto& intent=request.ability_edit;intent.kind=0;intent.id=1;intent.slot=0;intent.adjustment=real::AbilityAdjustment{cost,0,remove?(cost?4.f:-3.f):(cost?-1.f:1.f)};
    fixture_need(real::ability_adjusted_record(fixture_live.record,*intent.adjustment,intent.record),"fixture exact expected proposal failed");
    std::string error;fixture_need(quest_bridge::sha256(record_json(fixture_live.record).dump(),intent.before_sha256,error),error);request.canonical_record_key=real::ability_key(intent);return request;
}
std::string fixture_consumed(bool host_role,bool cost=false,bool remove=false,int defect=0,tick::Receipt* completed=nullptr){
    fixture_reset(host_role);auto request=fixture_request(cost,remove);fixture_defect=defect;fixture_controller["pending"]=true;
    fixture_proposal={0x100,0x999,0x888,1,17,std::nanf("")};const auto saved_ui=fixture_proposal;
    adjustment_views[fixture_id]=0x777;
    tick::Config hc;hc.context=a::fixture_selected.value;hc.context.local_peer=1;hc.context.role=real::Role::Host;
    tick::Config cc;cc.context=a::fixture_selected.value;auto validate=[](const tick::Input&,std::string&){return true;};
    tick::Queue host(hc,validate),client(cc,validate);tick::Input input{77,fixture_snapshot,request};tick::Assigned assigned;tick::Batch batch;tick::Receipt receipt;std::string error,digest;
    fixture_need(host.offer(2,input,assigned,error)==tick::Admission::Accepted&&host.seal(batch,error),error);
    if(!host_role)fixture_need(client.admit(1,batch,error)==tick::Admission::Accepted,error);
    auto& queue=host_role?host:client;FixtureSession session;FixtureEngine engine;
    const bool consumed=queue.consume(1,[](std::string&){return true;},[&](const tick::Assigned& command,std::string& why){return tick::authority::run(queue,session,engine,fixture_binding,command,[&](std::string& cause_value){
        const auto& actual=std::get<real::Request>(command.input.command);auto wrong=actual;wrong.sequence++;
        fixture_need(perform(fixture_binding,wrong,nullptr,cause_value)==a::Outcome::RejectedNoEffect&&!fixture_originals,"Different consumed Ability command reached original");
        auto other_binding=fixture_binding;++other_binding.native_generation;
        fixture_need(perform(other_binding,actual,nullptr,cause_value)==a::Outcome::RejectedNoEffect&&!fixture_originals,"Different native Ability generation reached original");
        Config::instance().is_host=!host_role;
        fixture_need(perform(fixture_binding,actual,nullptr,cause_value)==a::Outcome::RejectedNoEffect&&!fixture_originals,"Different actual Ability role reached original");Config::instance().is_host=host_role;
        a::Evidence adopted;fixture_need(perform(fixture_binding,actual,&adopted,cause_value)==a::Outcome::RejectedNoEffect&&!fixture_originals,"Ordered Ability accepted Host evidence");
        if(host_role&&!cost&&!remove&&!defect){const auto protected_before=fixture_proof;
            for(const auto* field:{"actors_sha256","bosses_sha256","release_sha256","other_definitions_sha256","other_actors_sha256","quests_sha256","art_store_sha256"}){
                fixture_proof.erase(field);fixture_need(perform(fixture_binding,actual,nullptr,cause_value)==a::Outcome::RejectedNoEffect&&!fixture_originals,"Missing complete protected Ability field reached original");
                fixture_proof=protected_before;fixture_proof[field]="bad";
                fixture_need(perform(fixture_binding,actual,nullptr,cause_value)==a::Outcome::RejectedNoEffect&&!fixture_originals,"Invalid protected Ability digest reached original");fixture_proof=protected_before;
            }}
        const auto outcome=perform(fixture_binding,actual,nullptr,cause_value);
        fixture_need(!fixture_seeds&&!fixture_ui_originals,"Ordered adjustment seeded RNG or adopted local presentation");
        if(defect){fixture_need(outcome==a::Outcome::Unknown,"Partial/unrelated original adjustment confirmed: "+cause_value);
            const auto calls=fixture_originals;fixture_need(perform(fixture_binding,actual,nullptr,cause_value)==a::Outcome::Unknown&&fixture_originals==calls,"Unknown original adjustment replayed");return outcome;}
        fixture_need(outcome==a::Outcome::Confirmed,cause_value);fixture_need(fixture_originals==1&&fixture_proposal==saved_ui,"Recorded intent depended on transient proposal or failed exact local restoration");
        a::Evidence evidence;fixture_need(observe(fixture_binding,actual,evidence,cause_value)&&retire(fixture_binding,actual,cause_value)&&observe(fixture_binding,actual,evidence,cause_value),cause_value);
        tick::ExecutionReceipt executed;executed.command=command;executed.outcome=outcome;executed.evidence=std::make_shared<a::Evidence>(evidence);
        fixture_need(tick::canonical_evidence(a::fixture_selected.value,command,executed,digest,cause_value),cause_value);
        fixture_need(queue.bind_evidence(command,digest,cause_value),cause_value);return outcome;
    },why);},[](uint64_t,std::string&){return a::Outcome::Confirmed;},receipt,error);
    fixture_need(consumed==!defect,"Queue outcome differs from actual Ability result: "+error);
    fixture_need(!tick::authority::active(),"Ability consumed authority escaped command");if(completed)*completed=receipt;return digest;
}
void fixture_cohort(bool cost,bool remove,const std::string& digest,const tick::Receipt& independent_client){
    fixture_reset();auto request=fixture_request(cost,remove);tick::Config config;config.context=a::fixture_selected.value;
    auto validate=[](const tick::Input&,std::string&){return true;};auto current=[](std::string&){return true;};auto advance=[](uint64_t,std::string&){return a::Outcome::Confirmed;};
    for(int mode=0;mode<3;++mode){tick::Queue queue(config,validate);tick::Input input{77,fixture_snapshot,request};tick::Assigned assigned;tick::Batch batch;tick::Receipt receipt;std::string error;
        fixture_need(queue.offer(2,input,assigned,error)==tick::Admission::Accepted&&queue.seal(batch,error),error);
        const bool consumed=queue.consume(1,current,[&](const tick::Assigned& command,std::string& why){if(mode&& !queue.bind_evidence(command,digest,why))return a::Outcome::Unknown;return a::Outcome::Confirmed;},advance,receipt,error);
        if(!mode){fixture_need(!consumed&&receipt.outcome==a::Outcome::Unknown,"Missing Ability canonical evidence retired a consumed command");continue;}
        fixture_need(consumed&&queue.state().evidence_ack_mask!=3,"Ability effect retired before full cohort ACK");
        auto peer=independent_client;if(mode==2)peer.command_evidence.front().sha256=std::string(64,'0');
        if(mode==1)fixture_need(queue.acknowledge(2,peer,error)&&queue.state().evidence_ack_mask==3,"Independent equal Ability proof did not complete cohort ACK: "+error);
        else fixture_need(!queue.acknowledge(2,peer,error)&&queue.state().locked,"Different Ability canonical evidence reached cohort ACK");
    }
}
void fixture_run(){
    fixture_reset();std::string error;bool handled=false;fixture_go=true;
    fixture_need(capture_adjustment_impl(reinterpret_cast<void*>(0x100),const_cast<char*>("DoAbilityAdjustment"),handled,error)&&handled&&pending&&pending->context&&captures==1&&!fixture_originals,error);
    fixture_go=false;fixture_proposal={0x100,0x999,0x888,1,17,3};tick();fixture_need(fixture_submissions==1&&!pending&&!fixture_originals,"Held Ability capture lost Context or reread incidental UI");tick();fixture_need(fixture_submissions==1,"Held Ability intent submitted twice");
    for(int drift=0;drift<3;++drift){fixture_reset();fixture_go=true;fixture_need(capture_adjustment_impl(reinterpret_cast<void*>(0x100),const_cast<char*>("DoAbilityAdjustment"),handled,error),error);fixture_go=false;
        if(drift==0)++a::fixture_selected.value.session;if(drift==1)fixture_binding_fresh=false;if(drift==2)fixture_controller["ordered"]=false;tick();fixture_need(!fixture_submissions&&!pending&&!fixture_originals,"Stale captured Ability Context/Binding/mode submitted");}
    fixture_reset();auto request=fixture_request();fixture_controller["pending"]=true;
    fixture_need(perform(fixture_binding,request,nullptr,error)==a::Outcome::RejectedNoEffect&&!fixture_originals,"Unconsumed ordered Ability reached original");
    for(bool cost:{false,true})for(bool remove:{false,true}){tick::Receipt peer;const auto host=fixture_consumed(true,cost,remove),client=fixture_consumed(false,cost,remove,0,&peer);fixture_need(!host.empty()&&host==client,"Independent Host/Client canonical Ability effect differs");fixture_cohort(cost,remove,host,peer);}
    for(int defect=1;defect<=8;++defect){fixture_consumed(true,false,false,defect);fixture_consumed(false,false,false,defect);}
    fixture_reset();auto meta=fixture_request();meta.ability_edit.adjustment.reset();tick::Input input{77,fixture_snapshot,meta};fixture_need(tick::requires_evidence(input),"Live positive-effect Apply lacks ordered evidence");
}
}
int main()try{fixture_ability::fixture_run();std::cout<<fixture_ability::fixture_checks<<" ordered original Ability adjustment capture/consumed/effect/canonical checks passed\n";return 0;}catch(const std::exception& why){std::cerr<<why.what()<<'\n';return 1;}
