#include "typed_action/action_session.h"
#include <optional>
#define main fixture_offered_controls_regression
#include "test_character_offered_controls.cpp"
#undef main
namespace a=mt2mp::action_session;
using json=nlohmann::json;
struct UiJob {json arguments,result=json::object();a::Binding binding;struct {std::string snapshot_uuid="snapshot";}context;
    bool waiting=false,attempted=false;unsigned stage=0,waits=0;};
std::vector<OfferedControl> fixture_controls;
std::string active_ui_token;
unsigned fixture_mode=0,fixture_calls=0,fixture_opens=0,fixture_icon_opens=0;
int fixture_gameplay=0;
struct Sample {int value=0;};
bool sample(Memory&,const a::Binding&,const std::string&,const a::CharacterRenameIntent&,Sample& out,std::string&){out.value=fixture_gameplay;return true;}
bool same_local(const Sample&,const Sample&){return fixture_mode!=7;}
bool code(Memory&,std::string& error){return fixture_mode!=14||fail(error,"fixture exact code cause");}
bool edit_code(Memory&,std::string&){return true;}
bool edit_open(UiJob&,std::string&){++fixture_opens;return true;}
bool edit_target(Memory&,const json&,uintptr_t& view,uintptr_t& named,uintptr_t& owner,std::string&){view=0x2000;named=0x1000;owner=0x3000;return true;}
bool edit_page(UiJob& job,uintptr_t,uintptr_t,const std::string&,const std::string&,bool,bool& current,std::string&){
    current=fixture_mode!=13;if(!current)job.waiting=true;return true;
}
namespace costume_edit {
struct Pending {a::CostumeEditIntent intent;std::string diagnostic_token;};
struct Sample {int value=0;};std::optional<Pending> pending;unsigned captures=0;bool fault=false;
bool window(Memory&,uintptr_t& outer,uintptr_t& live,uintptr_t& draft,uintptr_t& owner,std::string&){outer=0x2000;live=1;draft=2;owner=0x3000;return true;}
bool code(Memory&,std::string&){return true;}
bool sample(Memory&,const a::Binding&,const std::string&,const a::CostumeEditIntent&,Sample& out,std::string&){out.value=fixture_gameplay;return true;}
bool same_local(const Sample&,const Sample&){return fixture_mode!=7;}
}
namespace ability_edit {
struct Pending {a::AbilityEditIntent intent;uintptr_t view=0;std::string diagnostic_token;};
std::optional<Pending> pending;unsigned captures=0;bool fault=false;
}
bool costume_offered(Memory&,const json&,uintptr_t& outer,uintptr_t& named,std::vector<OfferedControl>& out,std::string&){
    outer=0x2000;named=0x1000;out=fixture_controls;return true;
}
json costume_controls(const std::vector<OfferedControl>&){return json::array();}
bool ability_offered(Memory&,const json& args,uintptr_t& outer,json& details,std::vector<OfferedControl>& out,std::string&){
    outer=0x1000;details={{"kind",args.at("kind")},{"id",args.at("id")},{"slot",args.at("slot")},{"positive_effects",fixture_mode!=9}};out=fixture_controls;return true;
}
bool ability_icon(Memory&,uintptr_t,uintptr_t,uintptr_t,int32_t,OfferedControl& out,std::string&){out.pane=0x9000;out.ready=out.icon=true;return true;}
bool ability_catalogue(Memory&,uintptr_t,uintptr_t,uintptr_t,json& result,std::string&,json* census=nullptr){if(census)*census={{"catalogue_ready",fixture_mode!=15},{"rebuild_pending",fixture_mode==15}};result=json::array({{{"slot",2},{"uid",123},{"name","Real offer"},{"level",1},{"positive_effects",true}}});return true;}
void mutate(){if(fixture_mode==5)++fixture_gameplay;}
uint32_t edit_checkbox(uintptr_t pane){
    ++fixture_calls;if(fixture_mode==8)return 123;
    put(pane,0x346,1);put(pane,0x344,0);mutate();
    if(pane==0x4000){costume_edit::Pending saved;saved.intent.kind=0;saved.intent.id=fixture_mode==4?9:1;
        saved.intent.weapon_category=a::CostumeWeaponIntent{fixture_mode==2?"Sword":"Staff",true};saved.diagnostic_token=active_ui_token;
        costume_edit::pending=saved;costume_edit::captures+=fixture_mode==3?2:1;}
    return 0;
}
uint32_t edit_activate(uintptr_t,bool icon=false){
    ++fixture_calls;if(icon){++fixture_icon_opens;return 0;}if(fixture_mode==8)return 123;mutate();
    if(fixture_mode!=9){ability_edit::Pending saved;saved.intent.kind=0;saved.intent.id=fixture_mode==4?9:1;saved.intent.slot=2;
        saved.view=0x1000;saved.diagnostic_token=active_ui_token;ability_edit::pending=saved;ability_edit::captures+=fixture_mode==3?2:1;}
    return 0;
}
uint32_t edit_text(uintptr_t outer,uintptr_t pane,const char* text,size_t size,bool commit){
    ++fixture_calls;fixture_text[{pane,0x2d0}]=std::string(text,size);
    if(commit){fixture_text[{pane,0x4a8}]=std::string(text,size);fixture_text[{pane,0x4d8}]=std::string(text,size);}
    put(outer,0x3c0,commit?0:pane);put(pane,0x584,commit?0:1);mutate();return fixture_mode==8?123:0;
}
uint32_t edit_choice(uintptr_t pane,const char* text,size_t size){
    ++fixture_calls;put(pane,0x4a0,std::string_view(text,size)=="second"?1:0);mutate();return fixture_mode==8?123:0;
}
#include "character_offered_ability_driver_actual.inc"
#include "character_offered_costume_driver_actual.inc"
void reset_driver(){
    setup();fixture_mode=fixture_calls=fixture_opens=fixture_icon_opens=0;fixture_gameplay=0;active_ui_token.clear();
    costume_edit::pending.reset();ability_edit::pending.reset();costume_edit::captures=ability_edit::captures=0;costume_edit::fault=ability_edit::fault=false;
    OfferedControl checkbox;checkbox.pane=0x4000;checkbox.checkbox=checkbox.ready=true;checkbox.command="MessageTo Parent WeaponCategory Staff";
    fixture_controls={checkbox};
}
UiJob input(const char* operation){UiJob job;job.arguments={{"operation",operation},{"kind",0},{"id",1}};
    if(std::string(operation).find("Ability")!=std::string::npos)job.arguments["slot"]=2;
    if(std::string(operation)=="ToggleWeapon")job.arguments.update({{"category","Staff"},{"expected_enabled",false}});return job;}
int main()try{
    if(fixture_offered_controls_regression())return 1;
    unsigned checks=0;std::string error;const std::string token="fixture-token";
    const auto check=[&](bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(std::string(why)+" cause="+error);error.clear();};
    reset_driver();auto job=input("ToggleWeapon");active_ui_token=token;
    check(execute_edit_ui(job,token,error)&&job.attempted&&fixture_calls==1&&costume_edit::captures==1&&job.result.at("captured"),"exact original Costume capture");
    for(unsigned mode:{2u,3u,4u,5u,7u,8u}){reset_driver();job=input("ToggleWeapon");active_ui_token=token;fixture_mode=mode;
        check(!execute_edit_ui(job,token,error)&&job.attempted&&fixture_calls==1&&!error.empty(),"failed Costume receipt admitted");}
    reset_driver();job=input("ToggleWeapon");job.arguments["expected_enabled"]=true;
    check(!execute_edit_ui(job,token,error)&&!job.attempted&&!fixture_calls,"stale Costume expectation invoked native input");
    reset_driver();job=input("ToggleWeapon");fixture_controls[0].ready=false;
    check(!execute_edit_ui(job,token,error)&&!job.attempted&&!fixture_calls,"hidden Costume input invoked native");
    reset_driver();job=input("ShowCostume");check(!execute_edit_ui(job,token,error)&&job.waiting&&fixture_opens==1,"original Costume open stage");
    check(execute_edit_ui(job,token,error)&&job.stage==5&&fixture_opens==1&&!fixture_calls,"Costume page stages replayed opening");
    reset_driver();job=input("ApplyAbility");fixture_controls[0]={};fixture_controls[0].pane=0x7000;fixture_controls[0].ready=true;fixture_controls[0].command="MessageTo Parent Apply";
    check(execute_edit_ui(job,token,error)&&fixture_calls==1&&ability_edit::pending&&ability_edit::pending->diagnostic_token==token&&job.result.at("captured"),"exact Ability Apply input capture");
    for(unsigned mode:{3u,4u,5u,7u,8u}){reset_driver();job=input("ApplyAbility");fixture_mode=mode;fixture_controls[0]={};fixture_controls[0].pane=0x7000;fixture_controls[0].ready=true;fixture_controls[0].command="MessageTo Parent Apply";
        check(!execute_edit_ui(job,token,error)&&fixture_calls==1&&!error.empty(),"failed Ability Apply receipt admitted");}
    reset_driver();job=input("ApplyAbility");fixture_mode=9;fixture_controls[0]={};fixture_controls[0].pane=0x7000;fixture_controls[0].ready=true;fixture_controls[0].command="MessageTo Parent Apply";
    check(execute_edit_ui(job,token,error)&&fixture_calls==1&&!ability_edit::pending&&job.result.at("original_invalid_draft_validation"),"zero effects did not retain original validation");
    reset_driver();job=input("EditAbilityText");job.arguments.update({{"control","actual_text"},{"value","Changed Ä description"}});
    fixture_controls[0]={};fixture_controls[0].pane=0xb000;fixture_controls[0].ready=fixture_controls[0].text=true;fixture_controls[0].id="actual_text";
    for(auto offset:{size_t(0x2d0),size_t(0x4a8),size_t(0x4d8)})fixture_text[{0xb000,offset}]="Old draft";
    put(0xb000,0x584,0);for(size_t i=0;i<8;++i)put(0xb000,0x544+i*4,0);
    put(0xb000,0x440,80);
    check(execute_edit_ui(job,token,error)&&fixture_calls==1&&!ability_edit::captures&&!job.result.at("captured").get<bool>(),"actual text commit became an ordered action");
    fixture_calls=0;job=input("EditAbilityText");job.arguments.update({{"control","actual_text"},{"value","Uncommitted visible input"},{"commit",false}});
    check(execute_edit_ui(job,token,error)&&fixture_calls==1&&!ability_edit::captures&&!job.result.at("text_committed").get<bool>()&&
        fixture_values.at({0x1000,0x3c0})==0xb000&&fixture_values.at({0xb000,0x584})==1&&
        job.result.at("controls").at(0).at("value")=="Uncommitted visible input"&&
        job.result.at("controls").at(0).at("commit_baseline")=="Changed Ä description","uncommitted original text input lost its active baseline/focus");
    fixture_calls=0;job=input("EditAbilityText");job.arguments.update({{"control","actual_text"},{"value","Too long"}});put(0xb000,0x440,3);
    check(!execute_edit_ui(job,token,error)&&!fixture_calls&&!job.attempted,"Original native character limit was bypassed");
    reset_driver();job=input("EditAbilityChoice");job.arguments.update({{"control","actual_choice"},{"value","second"}});
    fixture_controls[0]={};fixture_controls[0].pane=0xc000;fixture_controls[0].ready=fixture_controls[0].combo=true;fixture_controls[0].id="actual_choice";
    put(0xc000,0x348,0xd000);put(0xc000,0x350,2);put(0xc000,0x354,2);put(0xc000,0x4a0,0);fixture_text[{0xd000,0}]="first";fixture_text[{0xd000,32}]="second";
    check(execute_edit_ui(job,token,error)&&fixture_calls==1&&!ability_edit::captures,"actual native catalogue choice");
    fixture_calls=0;job=input("EditAbilityChoice");job.arguments.update({{"control","actual_choice"},{"value","fabricated"}});
    check(!execute_edit_ui(job,token,error)&&!fixture_calls&&!job.attempted,"unoffered choice reached native input");
    job=input("EditAbilityText");job.arguments.update({{"control","actual_choice"},{"value","text"}});
    check(!execute_edit_ui(job,token,error)&&!fixture_calls&&!job.attempted,"wrong control type reached native input");
    reset_driver();job=input("ShowAbility");check(!execute_edit_ui(job,token,error)&&fixture_opens==1,"Ability genuine Character open");
    check(!execute_edit_ui(job,token,error)&&fixture_icon_opens==1&&job.waiting,"Ability actual icon did not activate");
    check(execute_edit_ui(job,token,error)&&fixture_icon_opens==1&&fixture_opens==1,"Ability input replayed original navigation");
    reset_driver();job=input("ShowAbility");job.arguments.erase("slot");
    check(!execute_edit_ui(job,token,error)&&fixture_opens==1,"Ability catalogue did not use original Character navigation");
    check(execute_edit_ui(job,token,error)&&!fixture_icon_opens&&job.result.at("catalogue_only").get<bool>()&&
        job.result.at("ability_slots").at(0).at("slot")==2&&job.result.at("ability_slots").at(0).at("positive_effects").is_boolean(),"Read-only actual slot census required a guessed slot or opened an editor");
    reset_driver();job=input("ShowAbility");job.arguments.erase("slot");fixture_mode=15;
    check(!execute_ability_ui(job,token,error)&&fixture_opens==1,"pending catalogue initial navigation");
    for(unsigned i=0;i<8;++i)check(!execute_ability_ui(job,token,error)&&job.waiting&&error.empty()&&!fixture_icon_opens,"pending catalogue did not wait for normal original updates");
    check(!execute_ability_ui(job,token,error)&&error.find("rebuild remained pending")!=std::string::npos&&fixture_opens==1&&!fixture_icon_opens,"pending catalogue timeout lost measured cause or replayed navigation");
    reset_driver();job=input("ShowAbility");job.arguments.erase("slot");fixture_mode=15;
    check(!execute_ability_ui(job,token,error),"pending catalogue open");check(!execute_ability_ui(job,token,error)&&job.waiting,"pending catalogue stage");
    fixture_mode=0;check(execute_ability_ui(job,token,error)&&!job.waiting&&job.result.at("catalogue_only").get<bool>()&&fixture_opens==1&&!fixture_icon_opens,"normal rebuild completion did not finish retained query once");
    std::cout<<checks<<" actual offered input driver assertions passed\n";return 0;
}catch(const std::exception& exception){std::cerr<<exception.what()<<'\n';return 1;}
