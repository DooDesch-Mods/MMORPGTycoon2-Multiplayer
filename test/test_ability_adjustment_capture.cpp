#include "ability_edit_fixture.h"
#include "ability_adjustment_view.h"
#include <iostream>
namespace mt2mp::action_session::ui_diagnostic {struct Api {uintptr_t label_equals=1;};}
namespace adjustment_capture_fixture {
namespace a=mt2mp::action_session;namespace ability_adjustment_view=mt2mp::game::ability_adjustment_view;
struct Memory {};
struct State {uintptr_t image=0x100000;unsigned thread=9;} state;
unsigned actual_thread=9,native_originals=0;bool ready=true,fault=false,replaying=false,admitted=true,code_ok=true,route_ok=true,read_ok=true,sample_ok=true,label_ok=true;
uint64_t captures=0;std::string cause;
unsigned GetCurrentThreadId(){return actual_thread;}
bool fail(std::string& error,const std::string& why){error=why;return false;}
bool reject(const std::string& error){cause=error;return false;}
namespace net {namespace session_wire {bool connected(){return admitted;}}std::string current_snapshot_id(){return snapshot_id;}}
namespace typed_action_runtime {nlohmann::json controller_status(){return {{"started",true},{"locked",false},{"pending",false}};}
bool network_binding(const std::string&,a::Binding& out,std::string&){out={3,4};return true;}}
namespace character_rename {bool pending_ui(){return false;}}
void* _ReturnAddress(){return nullptr;}
namespace character_membership {bool import_presentation(void*,void*,uintptr_t,bool& handled,std::string&){handled=false;return true;}}
namespace costume_edit {bool pending_ui(){return false;}}
namespace quest_list {uint32_t label(uintptr_t,void*,const char*,bool& out){out=label_ok;return 0;}}
struct Sample {uintptr_t prototype=0x300;struct Owner {uintptr_t selected=0x200;} character;a::AbilityEditState value;};
struct Pending {a::Binding binding;std::string snapshot;a::AbilityEditIntent intent;uintptr_t view=0;};
std::optional<Pending> pending;
struct ViewTail {bool armed=false,preserve=false;uintptr_t view=0;uint32_t calls=0;};ViewTail view_tail;
void selected_original(void*,void*){++native_originals;}
void(*original_view_select)(void*,void*)=&selected_original;
ability_adjustment_view::State observed{0x100,0x200,0x300,0,0,1};
bool code(Memory&,std::string& e){return code_ok||fail(e,"exact original code failed");}
bool view_route(Memory&,std::string& e){return route_ok||fail(e,"exact owned route failed");}
bool read_proposal(Memory&,uintptr_t,ability_adjustment_view::State& out,std::string& e){if(!read_ok)return fail(e,"exact proposal read failed");out=observed;return true;}
template<class T>bool field(Memory&,uintptr_t base,size_t offset,T& out,std::string& e){
    if(base==0x200&&offset==0x2c){out=T(0);return true;}if(base==0x200&&offset==0x30){out=T(1);return true;}
    if(base==0x200&&offset==0x130){out=T(0x400);return true;}if(base==0x200&&offset==0x138){out=T(1);return true;}
    if(base==0x400&&offset==0){out=T(0x300);return true;}return fail(e,"exact owner field failed");}
bool sample(Memory&,const a::Binding&,const std::string&,const a::AbilityEditIntent&,Sample& out,std::string& e){
    if(!sample_ok)return fail(e,"exact current full sample failed");out={};out.value=before();return true;}
namespace quest_bridge {bool sha256(const std::string&,std::string& out,std::string&){out=std::string(64,'d');return true;}}
}
namespace mt2mp::action_session::ui_diagnostic {bool resolve_api(adjustment_capture_fixture::Memory&,uintptr_t,const char*,Api&,std::string&){return true;}}
namespace adjustment_capture_fixture {
constexpr const char* exe_sha="fixture-owned-image";
using json=nlohmann::json;
#include "ability_adjustment_record_body.inc"
#include "ability_adjustment_capture_body.inc"
#include "ability_adjustment_tail_body.inc"
}
int main()try{using namespace adjustment_capture_fixture;unsigned checks=0;std::string error;bool handled=false;adjustment_mode=true;adjustment_cost=false;
    const auto check=[&](bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(std::string(why)+" cause="+error);};
    check(capture_adjustment_impl(reinterpret_cast<void*>(0x100),nullptr,handled,error)&&handled&&pending&&captures==1&&!native_originals,"Normal proposal was not captured before original live effect");
    check(pending->intent.adjustment==std::optional<a::AbilityAdjustment>{{false,0,1}}&&pending->intent.record.effects[0].values[0]==4,"Captured exact selected effect/delta/full target differs");
    check(!capture_adjustment_impl(reinterpret_cast<void*>(0x100),nullptr,handled,error)&&captures==1&&!native_originals,"Pending proposal replayed or recaptured");pending.reset();
    label_ok=false;check(capture_adjustment_impl(nullptr,nullptr,handled,error)&&!handled&&!pending,"Unrelated native command was intercepted");label_ok=true;
    observed.prototype=0;check(capture_adjustment_impl(reinterpret_cast<void*>(0x100),nullptr,handled,error)&&handled&&!pending,"Original empty proposal invented action");observed.prototype=0x300;
    for(unsigned scenario=0;scenario<6;++scenario){error.clear();if(scenario==0)code_ok=false;if(scenario==1)route_ok=false;if(scenario==2)read_ok=false;if(scenario==3)sample_ok=false;if(scenario==4)actual_thread=10;if(scenario==5)observed.index=99;
        check(!capture_adjustment_impl(reinterpret_cast<void*>(0x100),nullptr,handled,error)&&handled&&!pending&&!error.empty()&&!native_originals,"Failed owner/code/thread/current proposal admission entered native original");
        code_ok=route_ok=read_ok=sample_ok=true;actual_thread=9;observed.index=0;}
    view_tail={true,true,0x100,0};view_select_entry(reinterpret_cast<void*>(0x100),reinterpret_cast<void*>(0x200));
    check(view_tail.calls==1&&!native_originals&&!fault,"Remote exact original presentation tail was not isolated");
    view_select_entry(reinterpret_cast<void*>(0x100),reinterpret_cast<void*>(0x200));check(fault&&!native_originals&&cause.find("count")!=std::string::npos,"Duplicate scoped presentation tail was accepted");
    fault=false;view_tail={true,false,0x100,0};view_select_entry(reinterpret_cast<void*>(0x100),reinterpret_cast<void*>(0x200));check(native_originals==1&&view_tail.calls==1&&!fault,"Initiating peer original presentation tail suppressed");
    view_tail={};view_select_entry(reinterpret_cast<void*>(0x100),reinterpret_cast<void*>(0x200));check(native_originals==2,"Unrelated native View selection was intercepted");
    std::cout<<checks<<" actual Ability adjustment capture/original presentation boundary checks passed\n";return 0;
}catch(const std::exception& exception){std::cerr<<exception.what()<<'\n';return 1;}
