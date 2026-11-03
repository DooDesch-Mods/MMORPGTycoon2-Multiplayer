#include "dungeon_config_fixture.h"
#include "typed_action/ordered_building_family.h"
#include "ui_remove/diagnostic/ui_diagnostic.h"
#include "release_edit_state.h"
#include "dungeon_scene_key.h"
#include <iostream>
using json=nlohmann::json;
namespace a=mt2mp::action_session;
namespace action=mt2mp::action_session;
namespace release_edit=mt2mp::game::release_edit;
namespace dungeon_scene_key=mt2mp::game::dungeon_scene_key;
bool fail(std::string& error,const std::string& cause){error=cause;return false;}
unsigned checks=0,original_calls=0,seed_calls=0,submit_calls=0;
void check(bool okay,const std::string& cause){++checks;need(okay,cause);}
a::Context fixture_context{7,1,1,3,a::Role::Host,snapshot_id};
a::Binding fixture_binding{2,3};a::Request request_value;
bool scoped=true,scope_valid=true,inside_go=false,valid_session=true,valid_binding=true;
bool prepare_rng=false,mutate_census=false,mutate_undo=false,mutate_money=false,return_original=true,read_failure=false;
json fixture_controller;
namespace mt2mp::action_session {
struct FixtureSession {bool current(Context& out,std::string& error){if(!valid_session)return fail(error,"Original Session read failed");out=::fixture_context;return true;}};
FixtureSession& selected_session(){static FixtureSession session;return session;}
namespace tick_commands::authority {
bool active(){return scoped;}
bool matches(const Context& c,const Binding& b,const Request& r,std::string& error){return (scoped&&scope_valid&&c==::fixture_context&&b==::fixture_binding&&r==::request_value)||fail(error,"Consumed command differs");}
bool matches(const Binding& b,const Request& r,std::string& error){return matches(::fixture_context,b,r,error);}
}
}
struct Config {bool is_host=true,local_test=true;static Config& instance(){static Config config;return config;}};
unsigned GetCurrentThreadId(){return 7;}
uint64_t ui_clock=100;
uint64_t GetTickCount64(){return ui_clock;}
bool tab_visible=true,tab_enabled_value=true,tab_command_okay=true;
unsigned tab_commands=0;
namespace net {std::string current_snapshot_id(){return snapshot_id;}namespace session_wire {bool connected(){return true;}}}
namespace typed_action_runtime {
json controller_status(){return fixture_controller;}
bool original_go_admission(bool& inside,std::string&){inside=inside_go;return true;}
bool read_binding(const std::string& id,a::Binding& out,std::string& error){if(!valid_binding||id!=snapshot_id)return fail(error,"Native fixture_binding changed");out=fixture_binding;return true;}
bool network_binding(const std::string& id,a::Binding& out,std::string& error){if(inside_go)return fail(error,"Network fixture_binding inside original Go");return read_binding(id,out,error);}
bool submit_dungeon_config(const std::string&,const a::DungeonConfigIntent&,std::string&){++submit_calls;return true;}
}
namespace action_runtime {
#include "dungeon_capture_binding_actual.inc"
uint32_t seed(uintptr_t,const std::array<uint64_t,4>&){++seed_calls;return 0;}
std::string uuid(std::string&){return action_id;}
}
namespace ui_move_runtime {
namespace selection_move {bool history_intent(const a::Binding&,const a::Request&,a::SelectionMoveIntent&,std::string& error){return fail(error,"Unexpected selection route");}}
namespace duplicate_edit {bool history_targets(const a::Binding&,const a::Request&,std::map<uint64_t,std::string>&,std::string& error){return fail(error,"Unexpected duplicate route");}}
namespace group_edit {bool history_admitted(const a::Binding&,const a::Request&,std::string& error){return fail(error,"Unexpected group route");}}
}
struct EngineFixture {
    struct Source {a::Request request;};std::map<std::string,Source> buildings_,operations_;
    bool live_state_=false,live_port_=false,live_result_=false;unsigned routed=0;
    a::Outcome execute_action_native(const a::Binding&,const a::Request&,std::string&){++routed;return a::Outcome::Confirmed;}
    a::Outcome execute(const a::Binding& binding,const a::Request& request,std::string& error){
        #include "ordered_path_route_actual.inc"
        return a::Outcome::RejectedNoEffect;
    }
};
struct Memory {};
struct {uintptr_t image=0x10000000;unsigned thread=7;}state;
constexpr struct {uintptr_t rva=0x6aea60;}ui_code;
struct Local {uintptr_t dungeon=0x8000,exterior=0x6000;uint64_t uid=77;bool operator==(const Local&)const=default;};
struct Sample {Local local;a::DungeonConfigState value;a::Ledger money;unsigned undo=1;};
Sample current;
using Command=a::ui_diagnostic::UiCommand;
bool original_stub(void*,void*){++original_calls;return true;}
Command original=&original_stub;
bool ready=true,fault=false,replaying=false;
uintptr_t replay_receiver=0;uint32_t original_calls_local=0;
uint64_t captures=0,submitted=0;std::string cause,last_action,ui_token;
std::map<std::string,json> ui_jobs;
void tick_ui();
// The production name is also used for the exact intercepted replay count.
#define original_calls original_calls_local
#include "dungeon_config_types_actual.inc"
#undef original_calls
bool reject(const std::string& why){cause=why;return false;}
bool code(Memory&,std::string&){return true;}
bool same_local(const Sample& before,const Sample& after){return before.local==after.local&&before.undo==after.undo;}
bool sample(Memory&,const a::Binding&,const std::string&,const a::DungeonConfigIntent&,Sample& out,std::string& error){
    if(read_failure&&::original_calls)return fail(error,"Original Dungeon read failed");out=current;return true;
}
uintptr_t actual_window=0x9000,actual_owner=0x8000,actual_spinner=0x7000;
int32_t prepared_desired=4;
bool window(Memory&,uintptr_t& outer,uintptr_t& owner,int32_t& desired,int32_t& cost,std::string&){outer=actual_window;owner=actual_owner;desired=prepared_desired;cost=std::max(desired-current.value.desired,0)*current.value.price;return true;}
bool input_guard(Memory&,uintptr_t,uintptr_t& spinner,std::string&){spinner=actual_spinner;return true;}
uint32_t prepare(uintptr_t,uintptr_t,int32_t desired){prepared_desired=desired;if(prepare_rng)++current.value.global_random[0];return 0;}
template<class T>bool field(Memory&,uintptr_t base,size_t offset,T& value,std::string& error){
    if(base==0xc000&&offset==0x69){value=T(tab_visible?1:0);return true;}
    if constexpr(sizeof(T)>=4){
    if(base==state.image&&offset==0x13df630){value=T(0xa000);return true;}
    if(base==actual_owner&&offset==0x6f8){value=T(0x6000);return true;}
    if(base==0x6000&&offset==0x18){value=T(77);return true;}
    if(base==0xd000&&offset==0x410){value=T(tab_visible?0xc000:0);return true;}
    }
    return fail(error,"Unexpected native field");
}
namespace remove_diagnostics {bool resolve_current_building_key(const std::string&,uintptr_t,uint64_t,std::string& key,std::string&){key=intent().key;return true;}}
namespace quest_list {uint32_t label(a::ui_diagnostic::RecordEquals,void*,const char*,bool& matches){matches=true;return 0;}}
namespace building_status {bool pending_ui(){return false;}}
namespace dungeon_light {bool pending_ui(){return false;}}
namespace dungeon_sound {bool pending_ui(){return false;}}
namespace dungeon_gizmo {bool pending_ui(){return false;}}
namespace dungeon_palette {bool pending_ui(){return false;}}
namespace quest_list {bool pending_ui(){return false;}}
namespace character_rename {bool pending_ui(){return false;}}
namespace ability_edit {bool pending_ui(){return false;}}
namespace costume_edit {bool pending_ui(){return false;}}
unsigned opened_ui=0,spinner_ui=0,accepted_ui=0,selected_ui=0;
bool offered_ui=true,controls_valid=true;
bool entry(void*,void*);
namespace mt2mp::action_session::ui_diagnostic {
bool resolve_api(Memory&,uintptr_t,const std::string&,Api&,std::string&){return true;}
bool read_live_window(Memory&,const Context&,Window& window,std::string&){window.window=0xb000;return true;}
struct FixtureCalls:Calls {
bool select(const Api&,const Context&,Receipt&,std::string&)override{++selected_ui;return true;}
bool command(const Api& api,const Window& window,Receipt& receipt,std::string&)override{
    const std::string label=api.destroy_label;
    if(label=="SetPage \"{dungeoninfo_tab_instances}\""){++tab_commands;receipt.ui_command_returned=receipt.record_cleanup_confirmed=tab_command_okay;return tab_command_okay;}
    if(label=="OpenInstancesConfig"){++opened_ui;prepared_desired=current.value.desired;receipt.ui_command_returned=receipt.record_cleanup_confirmed=true;return true;}
    if(label=="SpinnerIncrement"||label=="SpinnerDecrement"){++spinner_ui;prepared_desired+=label=="SpinnerIncrement"?1:-1;receipt.ui_command_returned=receipt.record_cleanup_confirmed=true;return true;}
    if(label=="Accept"&&!replaying){++accepted_ui;receipt.ui_command_returned=entry(reinterpret_cast<void*>(window.window),nullptr);receipt.record_cleanup_confirmed=true;return receipt.ui_command_returned;}
    ++::original_calls;++original_calls_local;receipt.ui_command_returned=return_original;receipt.record_cleanup_confirmed=true;
    if(return_original){current.value.desired=prepared_desired;current.money=charged();
        if(mutate_census)current.value.instances.push_back({9,false});if(mutate_undo)++current.undo;if(mutate_money)++current.money.cash;}
    return true;
}};
Calls& process_calls(){static FixtureCalls calls;return calls;}
}
const std::string exe_sha="fixture";
#include "dungeon_config_admission_actual.inc"
#define original_calls original_calls_local
#include "dungeon_config_capture_actual.inc"
#include "dungeon_config_tick_actual.inc"
#include "dungeon_config_perform_actual.inc"
#include "dungeon_config_observe_actual.inc"
#undef original_calls
#include "dungeon_config_ui_values_actual.inc"
#include "dungeon_config_ui_read_actual.inc"
namespace dungeon_config_ui {
struct Controls {int32_t value=0;};
struct Tab {uintptr_t page=0xc000,view=0xd000;std::string label="{dungeoninfo_tab_instances}";};
template<class Read,class Text>bool prepare(Read&&,Text&&,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t,Tab&,std::string& error){return offered_ui||fail(error,"Actual original Instances tab owner differs");}
template<class Read,class Text>bool offer(Read&&,Text&&,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t,Tab&,std::string& error){return offered_ui||fail(error,"Actual offered Config owner differs");}
}
bool token_valid(const std::string& token){return token.size()==36;}
bool pending_ui(){return pending.has_value()||replaying;}
bool ui_codes(Memory&,std::string&){return true;}
uint32_t tab_enabled(uintptr_t,uintptr_t,bool& enabled){enabled=tab_enabled_value;return 0;}
bool tab_record_equals(const void*,const char*){return true;}
uint32_t find_control(uintptr_t,const char* name,size_t,uintptr_t& found){found=std::string(name)=="list"?0xc000:0xd000;return 0;}
bool controls(Memory&,uintptr_t,uintptr_t,uintptr_t& spinner,dungeon_config_ui::Controls& value,std::string& error){spinner=actual_spinner;value.value=prepared_desired;return controls_valid||fail(error,"Actual Spinner or Accept differs");}
namespace action_runtime {bool property_ui_text(Memory&,uintptr_t,std::string&,std::string&){return true;}}
#include "dungeon_config_ui_native_actual.inc"
void reset(bool host=true){
    ::original_calls=original_calls_local=seed_calls=submit_calls=0;
    scoped=scope_valid=valid_session=valid_binding=true;inside_go=false;
    prepare_rng=mutate_census=mutate_undo=mutate_money=read_failure=false;return_original=true;
    Config::instance().is_host=host;fixture_context={7,host?uint64_t(1):uint64_t(2),1,3,host?a::Role::Host:a::Role::Client,snapshot_id};
    fixture_controller={{"ordered",true},{"started",true},{"pending",true},{"locked",false},{"action_uuid",action_id}};
    current={};current.value=before();current.money=money();prepared_desired=desired_count;
    ready=true;fault=replaying=false;pending.reset();operations.clear();cause.clear();ui_jobs.clear();ui_token.clear();
    ui_preparation.reset();ui_resuming=false;ui_clock=100;
    tab_visible=tab_enabled_value=tab_command_okay=true;tab_commands=0;
    offered_ui=controls_valid=true;opened_ui=spinner_ui=accepted_ui=selected_ui=0;
    request_value=request().request;request_value.action_uuid=action_id;
}
void runtime_cases(){std::string error;
    for(bool host:{false,true})for(auto counts:{std::pair{1,1},std::pair{1,4},std::pair{4,1}})for(bool sandbox_mode:{false,true})for(int32_t price:{0,100}){
        previous_count=counts.first;desired_count=counts.second;sandbox=sandbox_mode;instance_price=price;reset(host);
        check(perform(fixture_binding,request_value,nullptr,error)==a::Outcome::Confirmed,error);
        check(::original_calls==1&&original_calls_local==1&&!seed_calls&&operations.at(action_id).ordered&&!operations.at(action_id).seed_attempted,"Ordered original once or no-seed receipt differs");
        a::Evidence evidence;check(observe(fixture_binding,request_value,evidence,error),error);
        check(evidence.dungeon_config_before==before()&&evidence.dungeon_config_after.desired==desired_count&&evidence.before==money()&&evidence.after==charged(),"Complete before/after config/Ledger omitted");
        check(retire(fixture_binding,request_value,error)&&observe(fixture_binding,request_value,evidence,error)&&evidence.retirement_confirmed,error);
        check(perform(fixture_binding,request_value,nullptr,error)==a::Outcome::Unknown&&::original_calls==1,"Duplicate config replayed");
        scope_valid=false;check(!observe(fixture_binding,request_value,evidence,error),"Observation escaped consumed command");
    }
    previous_count=1;desired_count=4;sandbox=false;instance_price=100;
    for(int mode=0;mode<5;++mode){reset();
        if(mode==0)scoped=false;if(mode==1)scope_valid=false;if(mode==2)fixture_context.role=a::Role::Client;if(mode==3)valid_session=false;
        a::Evidence host;check(perform(fixture_binding,request_value,mode==4?&host:nullptr,error)==a::Outcome::RejectedNoEffect&&::original_calls==0&&seed_calls==0,"Unscoped/foreign/Host state execution mutated");
    }
    for(int mode=0;mode<6;++mode){reset();
        if(mode==0)prepare_rng=true;if(mode==1)mutate_census=true;if(mode==2)mutate_undo=true;if(mode==3)mutate_money=true;if(mode==4)return_original=false;if(mode==5)read_failure=true;
        check(perform(fixture_binding,request_value,nullptr,error)==a::Outcome::Unknown&&!error.empty()&&!seed_calls,"Original partial mismatch escaped Unknown");
        const auto calls=::original_calls;check(perform(fixture_binding,request_value,nullptr,error)==a::Outcome::Unknown&&::original_calls==calls,"Unknown mutation replayed");
    }
    reset();check(perform(fixture_binding,request_value,nullptr,error)==a::Outcome::Confirmed,error);++current.money.cash;
    a::Evidence evidence;check(!observe(fixture_binding,request_value,evidence,error),"Changed Ledger accepted before ACK");
    reset();fixture_controller["pending"]=false;inside_go=true;scoped=false;
    check(entry(reinterpret_cast<void*>(actual_window),nullptr)&&pending&&pending->context==fixture_context&&!::original_calls,"Actual original-Go capture did not retain immutable Context");
    inside_go=false;tick();check(submit_calls==1&&!pending&&!fault,"Held config capture not submitted");
    reset();fixture_controller["pending"]=false;inside_go=true;scoped=false;
    check(entry(reinterpret_cast<void*>(actual_window),nullptr)&&pending,"Config capture failed");inside_go=false;++fixture_context.session;tick();
    check(!submit_calls&&fault&&!::original_calls,"Changed captured Session submitted");
    reset();EngineFixture engine;check(engine.execute(fixture_binding,request_value,error)==a::Outcome::Confirmed&&engine.routed==1,"Production ordered router rejected Dungeon config");
    auto other=request_value;other.action=a::Action::SetDungeonPalette;
    request_value=other;
    check(engine.execute(fixture_binding,other,error)==a::Outcome::RejectedNoEffect&&engine.routed==1,"Unaccepted Dungeon family admitted");
    reset();fixture_controller["pending"]=false;inside_go=true;scoped=false;ui_token="diagnostic-token";
    ui_jobs[ui_token]={{"key",intent().key},{"desired",desired_count},{"captured",false}};
    check(entry(reinterpret_cast<void*>(actual_window),nullptr)&&pending&&pending->token==ui_token&&ui_jobs.at(ui_token).value("captured",false),"Diagnostic token lost original Accept capture");
    const auto token=ui_token;ui_token.clear();inside_go=false;tick();check(submit_calls==1&&ui_jobs.at(token).value("submitted",false)&&ui_jobs.at(token).value("action_uuid",std::string{})==action_id,"Diagnostic token lost original ordered action");
    scoped=true;fixture_controller["pending"]=true;check(perform(fixture_binding,request_value,nullptr,error)==a::Outcome::Confirmed,error);
    json result;check(read_ui({{"token",token}},result,error)&&result.at("execution").at("original_calls")==1&&result.at("canonical_before").at("desired")==previous_count&&
        result.at("canonical_after").at("desired")==desired_count&&result.at("ledger_after").at("cash")==charged().cash&&!result.at("execution").at("seed_attempted").get<bool>(),"Retained diagnostic boundary evidence incomplete");
    check(read_ui({{"token",token}},result,error)&&::original_calls==1&&submit_calls==1,"Diagnostic readback replayed input");
    check(!read_ui({{"token","missing"}},result,error)&&!read_ui({{"token",token},{"desired",1}},result,error),"Unknown or mutation-shaped diagnostic read accepted");
    for(bool read_only:{false,true})for(auto counts:{std::pair{1,4},std::pair{4,1},std::pair{1,10}}){
        previous_count=counts.first;desired_count=counts.second;reset();fixture_controller["pending"]=false;scoped=false;
        const std::string ui_id="44444444-4444-4444-8444-444444444444";json args={{"token",ui_id},{"key",intent().key},{"desired",desired_count},{"read_only",read_only}};
        check(native_ui(args,result,error),error);
        check(selected_ui==1&&opened_ui==1&&::original_calls==0&&seed_calls==0&&current.value==before()&&current.money==money(),"Original offered Config preparation mutated gameplay or skipped selection/open");
        check(spinner_ui==unsigned(read_only?0:std::abs(desired_count-previous_count))&&accepted_ui==unsigned(!read_only)&&pending.has_value()==!read_only,"Original Config control sequence differs");
        check(!native_ui(args,result,error)&&opened_ui==1,"Attempted Config UI token replayed");
        if(!read_only){tick();fixture_controller["pending"]=true;scoped=true;check(perform(fixture_binding,request_value,nullptr,error)==a::Outcome::Confirmed,error);
            check(read_ui({{"token",ui_id}},result,error)&&result.at("execution").at("confirmed")==true&&::original_calls==1,error);}
    }
    for(bool offer_failure:{false,true}){reset();fixture_controller["pending"]=false;scoped=false;offered_ui=!offer_failure;controls_valid=offer_failure;
        check(!native_ui({{"token","55555555-5555-4555-8555-555555555555"},{"key",intent().key},{"desired",desired_count},{"read_only",false}},result,error)&&
            !accepted_ui&&!::original_calls&&!pending,"Foreign Config offer/control invoked Accept");}
    for(int mode=0;mode<5;++mode){reset();fixture_controller["pending"]=false;scoped=false;tab_visible=false;
        const std::string ui_id="66666666-6666-4666-8666-666666666666";json args={{"token",ui_id},{"key",intent().key},{"desired",desired_count},{"read_only",false}};
        if(mode==3)tab_enabled_value=false;if(mode==4)tab_command_okay=false;
        const bool started=native_ui(args,result,error);
        if(mode>=3){check(!started&&!opened_ui&&!accepted_ui&&!pending&&!ui_preparation,"Disabled/failed original Instances tab reached Configure");continue;}
        check(started&&ui_preparation&&result.value("waiting_for_tab",false)&&tab_commands==1&&selected_ui==1&&!opened_ui&&!accepted_ui&&!pending,"Deferred original Instances tab was skipped");
        tick_ui();check(ui_preparation&&tab_commands==1&&selected_ui==1&&!opened_ui,"Waiting for normal original UI update replayed tab/selection");
        if(mode==0)tab_visible=true;if(mode==1)ui_clock=5200;if(mode==2)++fixture_context.session;
        tick_ui();
        if(mode==0){check(!ui_preparation&&pending&&opened_ui==1&&accepted_ui==1&&tab_commands==1&&!::original_calls,"Natural original tab transition did not continue Configure/Accept once");}
        else {check(fault&&!ui_preparation&&!pending&&!opened_ui&&!accepted_ui&&ui_jobs.at(ui_id).value("unknown",false),"Timed-out/foreign tab wait did not retain failure without replay");tick_ui();check(!opened_ui&&tab_commands==1,"Failed original tab wait replayed input");}
    }
}
int main(){try{runtime_cases();std::cout<<checks<<" production Dungeon config capture, execution, evidence and no-replay checks passed\n";return 0;}
catch(const std::exception& exception){std::cerr<<exception.what()<<'\n';return 1;}}
