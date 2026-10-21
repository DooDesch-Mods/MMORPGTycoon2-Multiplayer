#include "typed_action/action_session.h"
#include "ui_remove/diagnostic/ui_diagnostic.h"
#include <iostream>
#include <optional>
#include <stdexcept>
namespace weapon_capture_fixture {
namespace a=mt2mp::action_session;
using json=nlohmann::json;
constexpr const char* fixture_snapshot="22222222-2222-4222-8222-222222222222";
constexpr const char* exe_sha="fixture";
struct Model {unsigned mode=0,originals=0,dirty=0,dirty_reads=0;bool connected=true,started=true,locked=false,controller_pending=false;};
Model model;
struct State {unsigned thread=7;uintptr_t image=0x140000000;}state;
struct Memory final:a::ui_capture::Memory {
    bool copy(uintptr_t,void*,size_t,std::string& e)override{e="unexpected physical copy";return false;}
    bool region(uintptr_t,a::ui_capture::Region&,std::string& e)override{e="unexpected physical region query";return false;}
};
bool fail(std::string& e,const char* why){e=why;return false;}
bool ready=true,fault=false,replaying=false;uintptr_t replay_receiver=0;uint32_t original_calls=0;uint64_t captures=0;
unsigned GetCurrentThreadId(){return model.mode==1?8:7;}
std::string cause;
bool reject(const std::string& error){cause=error;return false;}
bool original_call(void*,void*){++model.originals;return true;}
auto original=&original_call;
struct Sample {uintptr_t costume=10;struct Character {uintptr_t selected=40;}character;a::CostumeEditState value;};
struct Pending {a::Binding binding;std::string snapshot;a::CostumeEditIntent intent;};
std::optional<Pending> pending;
namespace net {namespace session_wire {bool connected(){return model.connected;}}std::string current_snapshot_id(){return fixture_snapshot;}}
namespace typed_action_runtime {
json controller_status(){return {{"started",model.started},{"locked",model.locked},{"pending",model.controller_pending}};}
bool network_binding(const std::string& id,a::Binding& out,std::string& e){if(model.mode==2||id!=fixture_snapshot)return fail(e,"exact fixture binding failure");out={3,4};return true;}}
namespace building_status {bool pending_ui(){return model.mode==3;}}
namespace dungeon_config {bool pending_ui(){return false;}}
namespace dungeon_sound {bool pending_ui(){return false;}}
namespace dungeon_gizmo {bool pending_ui(){return false;}}
namespace dungeon_palette {bool pending_ui(){return false;}}
namespace dungeon_light {bool pending_ui(){return false;}}
namespace character_rename {bool pending_ui(){return false;}}
namespace ability_edit {bool pending_ui(){return false;}}
namespace quest_list {
bool pending_ui(){return false;}
uint32_t label(a::ui_diagnostic::RecordEquals,void* record,const char* text,bool& out){out=(uintptr_t(record)==1&&std::string(text)=="Apply")||(uintptr_t(record)==2&&std::string(text)=="WeaponCategory");return 0;}}
namespace quest_bridge {bool sha256(const std::string&,std::string& out,std::string&){out=std::string(64,'a');return true;}}
bool code(Memory&,std::string& e){return model.mode!=4||fail(e,"exact fixture code guard failure");}
bool window(Memory&,uintptr_t& outer,uintptr_t& live,uintptr_t& draft,uintptr_t& owner,std::string& e){
    if(model.mode==5)return fail(e,"exact fixture window failure");outer=1;live=10;draft=20;owner=model.mode==6?41:40;return true;}
template<class T>bool field(Memory&,uintptr_t base,size_t offset,T& out,std::string& e){
    if((base==40||base==41)&&offset==0x2c){out=0;return true;}if((base==40||base==41)&&offset==0x30){out=1;return true;}return fail(e,"unexpected fixture owner field");}
bool read_record(Memory&,uintptr_t object,a::CostumeRecord& out,std::string& e){
    if(model.mode==7)return fail(e,"exact fixture record read failed");out.name=object==10?"Live":"Local unsaved draft";return true;}
bool sample(Memory&,const a::Binding&,const std::string&,const a::CostumeEditIntent&,Sample& out,std::string&){
    out={};out.value.record.name="Live";out.value.weapon_categories={"Sword"};return true;}
json record_json(const a::CostumeRecord& value){return {{"name",value.name}};}
bool read_weapon_command(Memory&,uintptr_t,a::CostumeWeaponIntent& out,std::string& e){
    if(model.mode==8)return fail(e,"exact fixture command read failed");out={model.mode==9?"Sword":"Staff",true};return true;}
uint32_t weapon_dirty(uintptr_t outer){if(outer!=1)throw std::runtime_error("wrong original UI tail receiver");++model.dirty;return model.mode==10?123:0;}
bool weapon_dirty_readback(Memory&,uintptr_t,std::string& e){++model.dirty_reads;return model.mode!=11||fail(e,"exact fixture dirty readback failed");}
}
namespace mt2mp::action_session::ui_diagnostic {
bool resolve_api(ui::Memory&,uintptr_t image,std::string_view sha,Api& out,std::string& error,Family){
    if(weapon_capture_fixture::model.mode==12){error="exact fixture API resolution failed";return false;}
    if(image!=weapon_capture_fixture::state.image||sha!="fixture"){error="fixture actual API image/hash differs";return false;}
    out.label_equals=[](const void*,const char*){return false;};return true;
}
}
namespace weapon_capture_fixture {
#include "costume_weapon_capture_body.inc"
}
int main()try{using namespace weapon_capture_fixture;unsigned checks=0;const auto check=[&](bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(std::string(why)+" cause="+cause);};
    const auto reset=[](){model={};ready=true;fault=replaying=false;original_calls=0;captures=0;pending.reset();cause.clear();};
    reset();check(entry(reinterpret_cast<void*>(1),reinterpret_cast<void*>(2)),"Weapon capture failed");
    check(pending&&pending->intent.weapon_category==a::CostumeWeaponIntent{"Staff",true}&&pending->intent.record.name=="Live"&&captures==1,"Weapon captured wrong owner/live Costume/intent");
    check(model.originals==0&&model.dirty==1&&model.dirty_reads==1,"Weapon changed live gameplay or skipped original local UI tail");
    check(!entry(reinterpret_cast<void*>(1),reinterpret_cast<void*>(2))&&model.dirty==1&&model.originals==0&&captures==1,"Pending Weapon capture replayed");
    reset();check(entry(reinterpret_cast<void*>(1),reinterpret_cast<void*>(1))&&pending&&!pending->intent.weapon_category&&pending->intent.record.name=="Local unsaved draft"&&model.dirty==0,"Existing Apply draft capture changed");
    for(unsigned mode=1;mode<=12;++mode){reset();model.mode=mode;
        check(!entry(reinterpret_cast<void*>(1),reinterpret_cast<void*>(2))&&!pending&&captures==0&&!model.originals&&!cause.empty(),"Failed Weapon guard mutated or captured");
        if(mode==2||mode==4||mode==5||mode==7||mode==8||mode==11||mode==12)check(cause.starts_with("exact fixture"),"Original guard cause overwritten");}
    for(unsigned guard=0;guard<3;++guard){reset();if(guard==0)model.started=false;else if(guard==1)model.locked=true;else model.controller_pending=true;
        check(!entry(reinterpret_cast<void*>(1),reinterpret_cast<void*>(2))&&!pending&&!model.originals&&!model.dirty,"Unhealthy Controller admitted Weapon UI");}
    reset();model.connected=false;check(entry(reinterpret_cast<void*>(1),reinterpret_cast<void*>(2))&&model.originals==1&&!pending&&!model.dirty,"Standalone original behavior changed");
    std::cout<<checks<<" actual Costume entry pre-effect/owner/UI-tail/original-cause cases passed\n";return 0;
}catch(const std::exception& exception){std::cerr<<exception.what()<<'\n';return 1;}
