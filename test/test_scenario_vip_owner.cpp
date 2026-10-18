#include "toon_weapon_visual/authority.h"
#include "scenario_vip_authority/native.h"
#include "native_open_runtime.h"
#include "mt2mp/game/native_engine.h"
#include <Windows.h>
#include <atomic>
#include <iostream>
#include <stdexcept>
namespace action=mt2mp::action_session;
namespace fixture {
constexpr uintptr_t image=0x140000000;
struct Status {bool started=true,locked=false;};
struct Controller {action::Context context;action::Binding binding;Status value;
    Status status()const{return value;}const action::Context& bound_context()const{return context;}action::Binding bound_binding()const{return binding;}};
struct Session {action::Context context;bool valid=true;bool current(action::Context& out,std::string& e){if(!valid){e="actual Session unavailable";return false;}out=context;return true;}};
struct Bridge {bool is_locked=false;bool locked()const{return is_locked;}};
struct Fence {bool is_locked=false;std::string failure;bool locked()const{return is_locked;}const std::string& cause()const{return failure;}
    void journal_failed(const std::string& e){is_locked=true;failure=e;}};
Fence fence;
mt2mp::game::building_mixed_identity::opened_file::CurrentWorld world;
float scale=0;bool world_ok=true,clock_ok=true;uintptr_t unreadable=0,singleton=0,method=image+0x4f85b0;
unsigned reads=0,logs=0;
}
namespace placement_ui_probe {
bool ProcessMemory::copy(uintptr_t address,void* out,size_t size,std::string& error){
    ++fixture::reads;
    if(address==fixture::unreadable){error="actual memory read failed";return false;}
    if(address==fixture::image+0x13df330){std::memcpy(out,&fixture::singleton,size);return true;}
    if(address==fixture::image+0x1730e90+0xa0){std::memcpy(out,&fixture::method,size);return true;}
    std::memcpy(out,reinterpret_cast<const void*>(address),size);return true;
}
}
namespace mt2mp::game::native_open_runtime {bool read_current_world(building_mixed_identity::opened_file::CurrentWorld& out,std::string& error){if(!fixture::world_ok){error="actual world unavailable";return false;}out=fixture::world;return true;}}
namespace mt2mp::game::native {bool read_clock(ClockState& out,std::string& e){if(!fixture::clock_ok){e="actual Clock unavailable";return false;}out.time_scale=fixture::scale;return true;}}
namespace mt2mp::game::live_host_state {fixture::Fence& economic_fence(){return fixture::fence;}}
#define LOG_ERROR(...) (++fixture::logs)
namespace mt2mp::game::typed_action_runtime {
struct Runtime {
    fixture::Controller controller;fixture::Session session;
    uint32_t thread=GetCurrentThreadId();bool lobby_retiring=false;
    std::unique_ptr<fixture::Bridge> live_state=std::make_unique<fixture::Bridge>();std::string live_error;
    scenario_vip_authority::Receipt vip_last,vip_failure;uint64_t vip_suppressed=0;
};
std::atomic<Runtime*> published_runtime=nullptr;
placement_ui_probe::ProcessMemory weapon_memory;
uintptr_t weapon_image=fixture::image;
#include "scenario_vip_authority/runtime.inc"
}
namespace rt=mt2mp::game::typed_action_runtime;
namespace vip=mt2mp::game::scenario_vip_authority;
namespace {
unsigned checks=0;
void need(bool okay,const char* cause){++checks;if(!okay)throw std::runtime_error(cause);}
template<class T>void put(void* object,size_t offset,T value){std::memcpy(static_cast<char*>(object)+offset,&value,sizeof value);}
struct World {
    std::array<unsigned char,0x200> state{};std::array<unsigned char,0x70> log{};
    std::array<unsigned char,0xb0> receiver{};std::array<uintptr_t,3> entries{};
    rt::Runtime runtime;
    World(){
        fixture::fence={};fixture::scale=0;fixture::world_ok=fixture::clock_ok=true;fixture::unreadable=0;
        fixture::method=fixture::image+0x4f85b0;fixture::reads=fixture::logs=0;
        put(receiver.data(),0,fixture::image+0x1730e90);entries[0]=uintptr_t(receiver.data());
        put(log.data(),0,fixture::image+0x16fd7b0);put(log.data(),0x20,uintptr_t(entries.data()));
        put(log.data(),0x28,int32_t(1));put(log.data(),0x2c,int32_t(3));
        put(state.data(),0x1e8,uintptr_t(log.data()));fixture::singleton=uintptr_t(log.data());
        runtime.controller.context={1,2,1,3,action::Role::Client,"common"};runtime.controller.binding={2,3};runtime.session.context=runtime.controller.context;
        fixture::world={};fixture::world.world_epoch=2;fixture::world.generation=3;fixture::world.uuid="local";fixture::world.state=state.data();
        rt::published_runtime=&runtime;
    }
    bool owner(vip::Receipt& r,std::string& e){return rt::vip_owner(uintptr_t(receiver.data()),r,e);}
    void rejected(const char* cause){vip::Receipt r;std::string e;need(!owner(r,e)&&r.owned&&e.find(cause)!=std::string::npos,cause);}
};
}
int main(){try{
    {World w;vip::Receipt r;std::string e;need(rt::vip_client_active()&&w.owner(r,e)&&r.owned,"current Client owner rejected");
        need(r.receiver==uintptr_t(w.receiver.data())&&r.log==uintptr_t(w.log.data())&&r.snapshot=="common"&&r.origin=="local"&&r.native_generation==3,"owner identity not retained");
        need(fixture::reads==9,"unexpected owner read cost or Subscriber census");}
    {World w;w.runtime.live_state.reset();vip::Receipt r;std::string e;need(w.owner(r,e)&&r.owned,"started pre-first-frame Client must be fenced");}
    {World w;w.runtime.controller.context.role=action::Role::Host;vip::Receipt r;std::string e;need(!rt::vip_client_active()&&w.owner(r,e)&&!r.owned&&!fixture::reads,"Host must not enter Client reader");}
    {World w;w.runtime.controller.value.started=false;vip::Receipt r;std::string e;need(!rt::vip_client_active()&&w.owner(r,e)&&!r.owned&&!fixture::reads,"unjoined must not enter Client reader");}
    {World w;w.runtime.lobby_retiring=true;vip::Receipt r;std::string e;need(!rt::vip_client_active()&&w.owner(r,e)&&!r.owned&&!fixture::reads,"retired Runtime must not own old world");}
    {World w;w.runtime.thread=0;w.rejected("Mainthread");}
    {World w;w.runtime.controller.value.locked=true;w.rejected("authority locked");}
    {World w;w.runtime.live_state->is_locked=true;w.rejected("authority locked");}
    {World w;fixture::fence.journal_failed("first fence");w.rejected("first fence");}
    {World w;w.runtime.live_error="first failure";w.rejected("first failure");}
    {World w;w.runtime.session.valid=false;w.rejected("actual Session unavailable");}
    {World w;w.runtime.session.context.snapshot_uuid="other";w.rejected("Session differs");}
    {World w;fixture::world_ok=false;w.rejected("actual world unavailable");}
    {World w;fixture::world.world_epoch=9;w.rejected("lifetime");}
    {World w;fixture::world.generation=9;w.rejected("lifetime");}
    {World w;fixture::clock_ok=false;w.rejected("actual Clock unavailable");}
    {World w;fixture::scale=1;w.rejected("Clock is not zero");}
    {World w;put(w.receiver.data(),0,uintptr_t(0));w.rejected("receiver class");}
    {World w;fixture::unreadable=uintptr_t(w.receiver.data());w.rejected("actual memory read failed");}
    {World w;fixture::method+=1;w.rejected("virtual Update");}
    {World w;fixture::singleton=0;w.rejected("ScenarioLog owner");}
    {World w;put(w.log.data(),0,uintptr_t(0));w.rejected("ScenarioLog owner");}
    {World w;put(w.log.data(),0x28,int32_t(-1));w.rejected("array bounds");}
    {World w;put(w.log.data(),0x28,int32_t(65));put(w.log.data(),0x2c,int32_t(65));w.rejected("array bounds");}
    {World w;put(w.log.data(),0x2c,int32_t(0));w.rejected("array bounds");}
    {World w;put(w.log.data(),0x20,uintptr_t(0));w.rejected("array bounds");}
    {World w;w.entries[0]=0;w.rejected("unique current");}
    {World w;w.entries[1]=w.entries[0];put(w.log.data(),0x28,int32_t(2));w.rejected("unique current");}
    {World w;fixture::unreadable=uintptr_t(w.entries.data());w.rejected("actual memory read failed");}
    {World w;put(w.receiver.data(),0x68,uintptr_t(0x1234));put(w.receiver.data(),0x98,uint8_t(1));vip::Receipt r;std::string e;
        need(w.owner(r,e)&&r.owned,"loaded started VIP must remain Host-authoritative");}
    {World w;vip::Receipt r;std::string e;need(w.owner(r,e),"status owner missing");r.suppressed=true;
        rt::vip_publish(r);rt::vip_publish(r);auto s=rt::vip_status(w.runtime);
        need(s["suppressed_updates"]==2&&s["last"]["suppressed"]==true&&s["failure"].is_null(),"suppression receipt lost");
        need(!fixture::fence.locked()&&w.runtime.live_error.empty()&&!fixture::logs,"successful suppression logged failure");}
    {World w;vip::Receipt r;r.owned=true;r.cause="first original cause";rt::vip_publish(r);r.cause="later cause";rt::vip_publish(r);
        need(w.runtime.live_error=="first original cause"&&fixture::fence.cause()=="first original cause"&&fixture::logs==1,"first failure was hidden or repeated");
        need(rt::vip_status(w.runtime)["failure"]["cause"]=="first original cause","failure receipt replaced");}
    {World w;vip::Receipt r;r.owned=true;rt::vip_publish(r);need(!w.runtime.live_error.empty(),"missing failure cause became success");}
    {World w;vip::Receipt r;r.cause="foreign";rt::vip_publish(r);need(w.runtime.live_error.empty()&&!fixture::fence.locked(),"unowned receipt changed authority");}
    std::cout<<checks<<" actual VIP Runtime owner/status checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
