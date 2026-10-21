#include <optional>
namespace uptime_owner_fixture {
namespace a {
enum class Role {Host,Client};
struct Context {Role role=Role::Client;std::string snapshot_uuid="shared";bool operator==(const Context&)const=default;};
struct Binding {uint64_t native_epoch=7,native_generation=11;};
Context fixture_context;bool fixture_session_ok=true;
struct Session {bool current(Context& out,std::string& e){out=fixture_context;if(!fixture_session_ok)e="fixture Session changed";return fixture_session_ok;}};
Session& selected_session(){static Session fixture_session;return fixture_session;}
}
namespace building_mixed_identity::opened_file {struct CurrentWorld {uint64_t world_epoch=7,generation=11;uintptr_t map=0x8000;};}
namespace native_open_runtime {
building_mixed_identity::opened_file::CurrentWorld fixture_world;bool fixture_world_ok=true;
bool read_current_world(building_mixed_identity::opened_file::CurrentWorld& out,std::string& e){out=fixture_world;if(!fixture_world_ok)e="fixture NativeOrigin changed";return fixture_world_ok;}
}
struct Runtime {DWORD thread=GetCurrentThreadId();uintptr_t image=0x140000000,gm_root=0x81c0;int memory=0;std::optional<a::Context> gm_context=a::Context{};std::optional<a::Binding> gm_binding=a::Binding{};};
constexpr uintptr_t fixture_object=0x20000;
uintptr_t fixture_root=0x81c0;bool fixture_class_ok=true,fixture_field_ok=true;
bool fail(std::string& e,const std::string& cause){e=cause;return false;}
bool ap(Runtime&,uintptr_t object,uintptr_t expected,std::string& e){return (fixture_class_ok&&object==fixture_object&&expected==0x16d93c0)||fail(e,"fixture Building class changed");}
bool at(Runtime&,uintptr_t object,size_t offset,uintptr_t& out,std::string& e){out=fixture_root;return (fixture_field_ok&&object==fixture_object&&offset==0x170)||fail(e,"fixture Building root unreadable");}
namespace live::apply_timing {
enum class PageBoundary {OriginalCall};
struct PageReadScope {explicit PageReadScope(PageBoundary){}};
}
namespace remove_diagnostics {
struct LocalTarget {bool normal_building=true,normal_scenery=false;uintptr_t object=fixture_object;uint64_t uid=123,world_epoch=7,world_generation=11;std::string snapshot_id="shared";};
LocalTarget fixture_target;bool fixture_target_ok=true,fixture_modular=false,fixture_modular_valid=true;unsigned fixture_target_reads=0;
bool building_uptime_target(const LocalTarget& target,bool& matched,std::string& e){matched=false;if(!fixture_modular_valid)return fail(e,"original Modular Plan owner changed");matched=target.normal_building||fixture_modular;return true;}
bool resolve_identity_destination(uintptr_t object,LocalTarget& out,std::string& e){++fixture_target_reads;out=fixture_target;return (fixture_target_ok&&object==fixture_object)||fail(e,"fixture canonical target changed");}
}
namespace building_uptime {
struct Native {};bool fixture_history_ok=true;
bool read(int&,uintptr_t,uintptr_t object,Native&,std::string& e){return (fixture_history_ok&&object==fixture_object)||fail(e,"fixture uptime layout changed");}
}
#include "building_uptime_owner_actual.inc"
void run(){
    for(unsigned fixture_case=0;fixture_case<24;++fixture_case){
        Runtime fixture_runtime;a::fixture_context={};a::fixture_session_ok=true;native_open_runtime::fixture_world={};native_open_runtime::fixture_world_ok=true;
        fixture_root=fixture_runtime.gm_root;fixture_class_ok=fixture_field_ok=true;remove_diagnostics::fixture_target={};remove_diagnostics::fixture_target_ok=true;
        remove_diagnostics::fixture_target_reads=0;building_uptime::fixture_history_ok=true;remove_diagnostics::fixture_modular=false;remove_diagnostics::fixture_modular_valid=true;
        auto& fixture_target=remove_diagnostics::fixture_target;
        if(fixture_case==1)fixture_runtime.thread=0;
        if(fixture_case==2)fixture_runtime.gm_context.reset();
        if(fixture_case==3)fixture_runtime.gm_binding.reset();
        if(fixture_case==4)a::fixture_context.snapshot_uuid="different";
        if(fixture_case==5)a::fixture_context.role=a::Role::Host;
        if(fixture_case==6)++native_open_runtime::fixture_world.world_epoch;
        if(fixture_case==7)++native_open_runtime::fixture_world.generation;
        if(fixture_case==8)++native_open_runtime::fixture_world.map;
        if(fixture_case==9)fixture_class_ok=false;
        if(fixture_case==10)fixture_field_ok=false;
        if(fixture_case==11)remove_diagnostics::fixture_target_ok=false;
        if(fixture_case==12)fixture_target.normal_building=false;
        if(fixture_case==13)fixture_target.normal_scenery=true;
        if(fixture_case==14)++fixture_target.object;
        if(fixture_case==15)fixture_target.uid=0;
        if(fixture_case==16)fixture_target.snapshot_id="different";
        if(fixture_case==17)++fixture_target.world_epoch;
        if(fixture_case==18)++fixture_target.world_generation;
        if(fixture_case==19)building_uptime::fixture_history_ok=false;
        if(fixture_case>=20){fixture_target.normal_building=false;remove_diagnostics::fixture_modular=true;}
        if(fixture_case==21)remove_diagnostics::fixture_modular_valid=false;
        if(fixture_case==22)++fixture_target.world_generation;
        if(fixture_case==23)building_uptime::fixture_history_ok=false;
        bool fixture_owned=false;std::string fixture_error;
        const bool fixture_result=current_owner(fixture_runtime,fixture_object+0xc60,fixture_owned,fixture_error);
        if(!fixture_case||fixture_case==20)need(fixture_result&&fixture_owned&&remove_diagnostics::fixture_target_reads==1,"exact Client Building owner was not admitted");
        else need(!fixture_result&&!fixture_owned&&!fixture_error.empty(),"changed Client Building owner was admitted or lost cause");
        if(fixture_case==21)need(fixture_error=="original Modular Plan owner changed","Modular uptime ownership failure cause lost");
    }
    Runtime fixture_runtime;a::fixture_context={};native_open_runtime::fixture_world={};fixture_class_ok=fixture_field_ok=true;
    fixture_root=0x9000;remove_diagnostics::fixture_target_reads=0;bool fixture_owned=true;std::string fixture_error;
    need(current_owner(fixture_runtime,fixture_object+0xc60,fixture_owned,fixture_error)&&!fixture_owned&&!remove_diagnostics::fixture_target_reads,"non-root Building entered canonical root fence");
}
}
namespace uptime_fence_fixture {
struct Runtime {uintptr_t image=0x140000000;bool gm_client=false,gm_suspended=false;};
constexpr uintptr_t building_return=0x40b7a5;
std::string fault;uint64_t suppressed=0,forwarded=0;
unsigned fixture_original_calls=0,fixture_owner_calls=0,fixture_failures=0;
bool fixture_owner_ok=true,fixture_owned=true,fixture_original_throw=false,fixture_owner_throw=false;
void fixture_original(void*,int32_t){++fixture_original_calls;if(fixture_original_throw)throw std::runtime_error("fixture original Add exception");}
void(*original_add)(void*,int32_t)=fixture_original;
void retain(const std::string& e){++fixture_failures;if(fault.empty())fault=e;}
bool current_owner(Runtime&,uintptr_t,bool& owned,std::string& e){
    ++fixture_owner_calls;if(fixture_owner_throw)throw std::runtime_error("fixture owner exception");
    if(!fixture_owner_ok){e="fixture current Building UID changed";return false;}owned=fixture_owned;return true;
}
#include "building_uptime_dispatch_actual.inc"
void reset(){fault.clear();suppressed=forwarded=fixture_original_calls=fixture_owner_calls=fixture_failures=0;
    fixture_owner_ok=fixture_owned=true;fixture_original_throw=fixture_owner_throw=false;original_add=fixture_original;}
void run(){
    Runtime fixture_runtime;void* const fixture_history=reinterpret_cast<void*>(0x4000);const auto fixture_site=fixture_runtime.image+building_return;
    reset();dispatch(nullptr,fixture_history,1,fixture_site);need(fixture_original_calls==1&&!fixture_owner_calls,"offline Add did not forward");
    reset();dispatch(&fixture_runtime,fixture_history,1,fixture_site);need(fixture_original_calls==1&&!fixture_owner_calls,"Host Add did not forward");
    fixture_runtime.gm_client=true;
    reset();dispatch(&fixture_runtime,fixture_history,7,fixture_site+1);need(fixture_original_calls==1&&!fixture_owner_calls,"another IntegerHistory caller was intercepted");
    reset();dispatch(&fixture_runtime,fixture_history,1,fixture_site);need(!fixture_original_calls&&fixture_owner_calls==1&&suppressed==1&&fault.empty(),"active Client root uptime was not fenced");
    reset();fixture_owned=false;dispatch(&fixture_runtime,fixture_history,1,fixture_site);need(fixture_original_calls==1&&fixture_owner_calls==1&&!suppressed,"non-root uptime did not forward");
    reset();fixture_runtime.gm_suspended=true;dispatch(&fixture_runtime,fixture_history,1,fixture_site);need(!fixture_original_calls&&!fixture_owner_calls&&suppressed==1,"suspended Client restarted uptime");fixture_runtime.gm_suspended=false;
    reset();fixture_owner_ok=false;dispatch(&fixture_runtime,fixture_history,1,fixture_site);
    need(!fixture_original_calls&&fault=="fixture current Building UID changed"&&fixture_failures==1,"failed current owner lost cause or ran original");
    dispatch(&fixture_runtime,fixture_history,1,fixture_site);need(fixture_owner_calls==1&&suppressed==2,"failed uptime guard replayed");
    reset();fixture_owner_throw=true;dispatch(&fixture_runtime,fixture_history,1,fixture_site);need(!fixture_original_calls&&fixture_failures==1&&!fault.empty(),"owner exception was hidden");
    reset();dispatch(&fixture_runtime,fixture_history,2,fixture_site);need(!fixture_original_calls&&fixture_failures==1,"invalid uptime producer value admitted");
    reset();fixture_owned=false;fixture_original_throw=true;bool fixture_threw=false;
    try{dispatch(&fixture_runtime,fixture_history,1,fixture_site);}catch(const std::exception&){fixture_threw=true;}
    need(fixture_threw&&fixture_original_calls==1&&!fixture_failures,"original non-root Add exception was swallowed");
    reset();
}
}
void uptime_fence_cases(){uptime_owner_fixture::run();uptime_fence_fixture::run();}
