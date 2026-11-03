#include <Windows.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
namespace a {
enum class AbilityMembershipCommand {Copy,Dock};
struct AbilityMembershipIntent {AbilityMembershipCommand command{};int32_t display_slot=-1,level=-1;};
struct Context {std::string snapshot_uuid="snapshot";int session=1;bool operator==(const Context&)const=default;};
struct Binding {int generation=0;bool operator==(const Binding&)const=default;};
struct Session {bool valid=true;Context context;bool current(Context& out,std::string& error){if(!valid){error="fixture Session unavailable";return false;}out=context;return true;}};
Session& selected_session(){static Session value;return value;}
}
namespace character_definition_identity {struct Definition {uintptr_t object=0;bool deleted=false;uint64_t uid=1;};struct Census {std::vector<Definition> definitions;};}
namespace character_membership_reader {struct Users {};}
bool canonical=true,connected=true,code_valid=true;
uintptr_t canonical_owner=0,return_address=0;
namespace typed_action_runtime {
namespace action=a;
struct Status {bool started=true,locked=false,pending=false;};
struct Controller {
    Status state;action::Context context;action::Binding binding{23};
    Status status()const{return state;}action::Context bound_context()const{return context;}
    action::Binding bound_binding()const{return binding;}bool ordered_mode()const{return true;}
};
struct Engine {bool valid=true;bool current(const action::Binding&,const std::string&,std::string& error){if(!valid){error="fixture current engine differs";return false;}return true;}};
struct Runtime {DWORD thread=GetCurrentThreadId();Controller controller;action::Session session;Engine engine;bool lobby_retiring=false,live_state=false,live_port=false;};
Runtime fixture_runtime;std::atomic<Runtime*> published_runtime{&fixture_runtime};
bool held_tutorial_session_read(action::Session& session,action::Context& out,std::string& error){return session.current(out,error);}
bool network_binding_with_mode(const std::string&,action::Binding&,std::string&,bool*);
#include "ability_presentation_binding_actual.inc"
}
namespace character_actor_records {
bool read_current(const a::Context&,const a::Binding&,character_definition_identity::Census& out,character_membership_reader::Users&,std::string&){
    if(canonical)out.definitions.push_back({canonical_owner,false,17});return true;
}}
namespace net::session_wire {bool connected(){return ::connected;}}
struct NativeState {uintptr_t image=0x140000000;DWORD thread=GetCurrentThreadId();} state;
struct Memory {
    bool copy(uintptr_t p,void* out,size_t bytes,std::string& error){
        __try{std::memcpy(out,reinterpret_cast<const void*>(p),bytes);return true;}
        __except(EXCEPTION_EXECUTE_HANDLER){error="fixture read failed";return false;}
    }
};
template<class T>bool field(Memory& memory,uintptr_t p,size_t offset,T& out,std::string& error){return memory.copy(p+offset,&out,sizeof out,error);}
bool fail(std::string& error,const std::string& cause){error=cause;return false;}
bool ready=true,fault=false,executing=false;
unsigned calls=0,queued=0,rejected=0;std::string cause;
using Command=a::AbilityMembershipCommand;using Dock=void(*)(void*,void*,float,bool);
Dock original_dock=nullptr;
bool code(Memory&,std::string& error){return code_valid||fail(error,"fixture pin differs");}
bool reject(const std::string& error){++rejected;cause=error;return false;}
bool queue(uintptr_t,a::AbilityMembershipIntent,uintptr_t,std::string&){++queued;return true;}
#define _ReturnAddress() reinterpret_cast<void*>(return_address)
#include "ability_presentation_actual.inc"
#undef _ReturnAddress
template<class T>void put(uintptr_t base,size_t offset,T value){std::memcpy(reinterpret_cast<void*>(base+offset),&value,sizeof value);}
alignas(16) std::array<unsigned char,0x500> owner_bytes{},slot_bytes{},icon_bytes{};
alignas(16) std::array<uintptr_t,2> proxy{},members{};
uintptr_t prototype=0,slot=uintptr_t(slot_bytes.data()),button=uintptr_t(icon_bytes.data());
enum class Mode {Success,PartialFailure,ReadOnlyCleanup,ExpiredCleanup,MissingDock};Mode mode=Mode::Success;
void original(void* receiver,void* icon,float seconds,bool animate){
    if(receiver!=reinterpret_cast<void*>(slot)||icon!=reinterpret_cast<void*>(button)||seconds!=0||!animate)throw std::runtime_error("native arguments changed");
    ++calls;put(prototype,0x208,int32_t(3));
    if(mode==Mode::PartialFailure)RaiseException(0xe0000011,0,0,nullptr);
    if(mode!=Mode::MissingDock)put(slot,0x380,button);
    if(mode==Mode::ExpiredCleanup)proxy[0]=0;
    if(mode==Mode::ReadOnlyCleanup){DWORD previous=0;if(!VirtualProtect(reinterpret_cast<void*>(prototype),4096,PAGE_READONLY,&previous))throw std::runtime_error("fixture protection failed");}
}
void setup(int32_t display=-1){
    DWORD previous=0;if(!VirtualProtect(reinterpret_cast<void*>(prototype),4096,PAGE_READWRITE,&previous))throw std::runtime_error("fixture writable restore failed");
    std::memset(reinterpret_cast<void*>(prototype),0,4096);owner_bytes={};slot_bytes={};icon_bytes={};
    canonical_owner=uintptr_t(owner_bytes.data());canonical=connected=code_valid=ready=true;fault=executing=false;calls=queued=rejected=0;cause.clear();mode=Mode::Success;
    original_dock=&original;return_address=state.image+0x652826;state.thread=GetCurrentThreadId();
    a::selected_session()={};typed_action_runtime::fixture_runtime={};typed_action_runtime::published_runtime.store(&typed_action_runtime::fixture_runtime);
    members={prototype,0};proxy={prototype+0x38,1};
    put(canonical_owner,0x130,uintptr_t(members.data()));put(canonical_owner,0x138,int32_t(1));put(canonical_owner,0x13c,int32_t(2));
    put(prototype,0,state.image+0x171c930);put(prototype,0x18,uint64_t(101));put(prototype,0x40,uintptr_t(proxy.data()));put(prototype,0x200,int32_t(1));put(prototype,0x208,display);
    put(slot,0,state.image+0x1717a00);put(slot,0x340,canonical_owner);put(slot,0x348,int32_t(3));put(slot,0x34c,int32_t(1));
    put(button,0,state.image+0x1717790);put(button,0x320,prototype);put(button,0x328,uintptr_t(proxy.data()));put(button,0x330,canonical_owner);put(button,0x399,uint8_t(0));
}
void invoke(){dock_entry(reinterpret_cast<void*>(slot),reinterpret_cast<void*>(button),0,true);}
int32_t display(){return *reinterpret_cast<const int32_t*>(prototype+0x208);}
int main()try{
    prototype=uintptr_t(VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));if(!prototype)throw std::runtime_error("fixture allocation failed");
    unsigned checks=0;auto check=[&](bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(std::string(why)+" cause="+cause);};
    for(const int32_t before:{-1,0,7,11}){setup(before);invoke();check(calls==1&&!queued&&!rejected&&!fault&&display()==before,"Stock rebuild did not dock once and preserve exact serialized slot");check(*reinterpret_cast<uintptr_t*>(slot+0x380)==button,"Original docked UI was not retained");}
    setup(7);typed_action_runtime::fixture_runtime.controller.state.pending=true;invoke();invoke();
    check(calls==2&&!queued&&!rejected&&!fault&&display()==7,"Pending ordered action prevented two original presentation docks or changed gameplay slot");
    {a::Binding binding;std::string error;check(!typed_action_runtime::network_binding("snapshot",binding,error)&&binding.generation==0&&error=="network admission requires idle unlocked Controller","Pending presentation allowance widened action admission");}
    setup();{a::Binding binding;std::string error;check(typed_action_runtime::network_binding("snapshot",binding,error)&&binding.generation==23,"Idle action admission changed");}
    auto refused=[&](const char* why){invoke();check(!calls&&!queued&&rejected==1&&fault,why);};
    setup();typed_action_runtime::published_runtime.store(nullptr);refused("Missing Controller was accepted");
    setup();typed_action_runtime::fixture_runtime.thread=0;refused("Foreign Controller thread was accepted");
    setup();typed_action_runtime::fixture_runtime.controller.state.started=false;refused("Stopped Controller was accepted");
    setup();typed_action_runtime::fixture_runtime.controller.state.locked=true;refused("Locked Controller was accepted");
    setup();typed_action_runtime::fixture_runtime.lobby_retiring=true;refused("Retiring lobby was accepted");
    setup();typed_action_runtime::fixture_runtime.session.valid=false;refused("Unavailable Controller Session was accepted");
    setup();a::selected_session().valid=false;refused("Unavailable selected Session was accepted");
    setup();typed_action_runtime::fixture_runtime.controller.context.session=2;refused("Changed Controller context was accepted");
    setup();a::selected_session().context.snapshot_uuid="other";refused("Changed selected snapshot was accepted");
    setup();typed_action_runtime::fixture_runtime.engine.valid=false;refused("Stale current engine was accepted");
    setup();ready=false;refused("Unready Ability runtime was accepted");
    setup();mode=Mode::PartialFailure;invoke();check(calls==1&&!queued&&rejected==1&&fault&&display()==-1,"Partial original failure did not restore slot and fail visibly");check(cause.find("original_seh=3758096401")!=std::string::npos&&cause.find("display_slot_restored=1")!=std::string::npos,"Original failure cause/cleanup evidence absent");
    setup(7);mode=Mode::ReadOnlyCleanup;invoke();check(calls==1&&!queued&&rejected==1&&fault&&display()==3,"Cleanup write failure did not remain failed");check(cause.find("cleanup_seh=3221225477")!=std::string::npos&&cause.find("display_slot_restored=0")!=std::string::npos,"Cleanup failure cause absent");
    setup();mode=Mode::ExpiredCleanup;invoke();check(calls==1&&fault&&rejected==1&&display()==3,"Expired weak owner was written during cleanup");
    setup();mode=Mode::MissingDock;invoke();check(calls==1&&fault&&rejected==1&&display()==-1,"Missing original UI docking was accepted");
    setup();return_address=state.image+0x652825;invoke();check(!calls&&queued==1&&!rejected&&!fault&&display()==-1,"Foreign caller entered presentation bypass");
    setup(3);return_address=state.image+0x100;invoke();check(calls==1&&!queued&&!rejected,"Existing same-slot user docking changed");
    setup();proxy[0]=prototype;invoke();check(!calls&&!queued&&rejected==1,"Unadjusted weak target was accepted");
    setup();put(prototype,0x40,uintptr_t(members.data()));invoke();check(!calls&&!queued&&rejected==1,"Nonreciprocal weak proxy was accepted");
    setup();proxy[1]=0;invoke();check(!calls&&!queued&&rejected==1,"Unleased icon weak proxy was accepted");
    setup();canonical=false;invoke();check(!calls&&!queued&&rejected==1&&fault,"Foreign Character owner was accepted");
    setup();members[0]=button;invoke();check(!calls&&!queued&&rejected==1,"Unowned prototype was accepted");
    setup();put(slot,0x34c,int32_t(2));invoke();check(!calls&&!queued&&rejected==1&&fault,"Presentation path allowed a gameplay level change");
    setup();put(button,0x399,uint8_t(1));invoke();check(!calls&&!queued&&rejected==1,"Prefab copy entered presentation path");
    setup();code_valid=false;invoke();check(!calls&&!queued&&rejected==1,"Changed native body was accepted");
    setup();state.thread=0;invoke();check(!calls&&!queued&&rejected==1,"Foreign thread was accepted");
    setup();connected=false;invoke();check(calls==1&&!queued&&!rejected&&display()==3,"Disconnected original path changed");
    setup();executing=true;invoke();check(calls==1&&!queued&&!rejected&&display()==3,"Consumed ordered original path changed");
    setup();if(!VirtualFree(reinterpret_cast<void*>(prototype),0,MEM_RELEASE))throw std::runtime_error("fixture release failed");
    std::cout<<"Ability presentation actual-body checks passed: "<<checks<<'\n';return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
