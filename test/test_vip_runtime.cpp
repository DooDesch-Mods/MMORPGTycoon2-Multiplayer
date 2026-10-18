#include "typed_action/action_session.h"
#include "live_host_state/vip_transitions.h"
#include <atomic>
#include <iostream>
#include <stdexcept>

namespace {
unsigned checks=0,logs=0;
uint32_t current_thread=7;
uint32_t GetCurrentThreadId(){return current_thread;}
template<class... T>void log_error(const char* text,const T&... values){(void)text;((void)values,...);++logs;}
void need(bool value,const char* cause){++checks;if(!value)throw std::runtime_error(cause);}
}
#define LOG_ERROR(...) log_error(__VA_ARGS__)
namespace mt2mp::net {
uint64_t epoch=9;
uint64_t current_world_epoch(){return epoch;}
}
namespace mt2mp::building_mixed_identity::opened_file {
struct CurrentWorld {uint64_t world_epoch=2,generation=3;};
}
namespace mt2mp::game::native_open_runtime {
building_mixed_identity::opened_file::CurrentWorld world;
bool available=true;
bool read_current_world(building_mixed_identity::opened_file::CurrentWorld& out,std::string& error){
    if(!available){error="fixture World unavailable";return false;}out=world;return true;
}
}
namespace mt2mp::game::live_host_state {
struct Fence {bool failed=false;bool locked()const{return failed;}} fence;
Fence& economic_fence(){return fence;}
}
namespace mt2mp::game::scenario_vip_authority::client_alert::cleanup_hook {
struct Callbacks {void(*before)(uintptr_t) noexcept;void(*after)(uintptr_t,bool) noexcept;};
unsigned installs=0,verifies=0;
bool result=true;
bool initialize(uintptr_t image,const Callbacks& callbacks,std::string& error){
    ++installs;if(!image||!callbacks.before||!callbacks.after||!result){error="fixture hook install failed";return false;}return true;
}
bool verify(std::string& error){++verifies;if(!result)error="fixture hook verification failed";return result;}
}
namespace mt2mp::game::typed_action_runtime {
namespace action=mt2mp::action_session;
struct FakeStatus {bool started=true,locked=false,pending=true;};
struct FakeController {
    FakeStatus value;
    action::Context context;
    action::Binding binding{2,3};
    const FakeStatus& status()const{return value;}
    const action::Context& bound_context()const{return context;}
    const action::Binding& bound_binding()const{return binding;}
};
struct FakeSession {
    action::Context context;bool available=true;
    bool current(action::Context& out,std::string& error){
        if(!available){error="fixture Session unavailable";return false;}out=context;return true;
    }
};
struct FakeLive {bool failed=false;bool locked()const{return failed;}};
struct FakePresentation {
    unsigned before=0,after=0;bool result=true,throws=false,abnormal=false;uintptr_t mode=0;
    bool before_deinit(uintptr_t actual,std::string& error){
        ++before;mode=actual;if(throws)throw std::runtime_error("fixture cleanup threw");
        if(!result)error="fixture cleanup Unknown";return result;
    }
    void after_deinit(uintptr_t actual,bool failed){++after;mode=actual;abnormal=failed;}
};
struct Runtime {
    uint32_t thread=7;bool lobby_retiring=false;
    FakeController controller;FakeSession session;
    FakeLive* live_state=nullptr;
    FakePresentation* vip_presentation=nullptr;
    std::string live_error;
};
std::atomic<Runtime*> published_runtime{nullptr};
uintptr_t weapon_image=0x140000000;
#include "scenario_vip_authority/alert_runtime.inc"
}

int main(){try{
    using namespace mt2mp::game;
    using namespace mt2mp::game::typed_action_runtime;
    const std::string uuid="11111111-2222-4333-8444-555555555555";
    action::Context context{77,2,1,3,action::Role::Client,uuid};
    action::Binding binding{2,3};
    // Host generation differs from the local Client generation.
    live_host_state::vip::Origin origin{77,9,18,uuid};
    Runtime runtime;FakeLive live;FakePresentation presentation;
    runtime.controller.context=context;runtime.session.context=context;
    runtime.live_state=&live;runtime.vip_presentation=&presentation;published_runtime=&runtime;
    std::string error;
    need(vip_alert_authorize(origin,context,binding,false,error),"pending exact action-before frame rejected");
    auto foreign=origin;foreign.session++;
    need(!vip_alert_authorize(foreign,context,binding,false,error),"foreign source session accepted");
    auto wrong_binding=binding;wrong_binding.native_generation++;
    need(!vip_alert_authorize(origin,context,wrong_binding,false,error),"wrong local binding accepted");
    current_thread=8;
    need(!vip_alert_authorize(origin,context,binding,true,error),"wrong teardown thread accepted");current_thread=7;
    runtime.controller.value.locked=true;
    need(!vip_alert_authorize(origin,context,binding,false,error),"locked Controller accepted for UI work");
    runtime.lobby_retiring=true;runtime.session.available=false;native_open_runtime::available=false;
    need(vip_alert_authorize(origin,context,binding,true,error),"retained exact teardown owner rejected after invalidation");
    need(!vip_alert_authorize(foreign,context,binding,true,error),"teardown bypassed exact Origin");
    runtime.controller.value.locked=false;runtime.lobby_retiring=false;runtime.session.available=true;native_open_runtime::available=true;
    auto saved=native_open_runtime::world;native_open_runtime::world.generation++;
    need(!vip_alert_authorize(origin,context,binding,false,error),"changed local World accepted");native_open_runtime::world=saved;
    live.failed=true;need(!vip_alert_authorize(origin,context,binding,false,error),"locked Frame accepted");live.failed=false;
    live_host_state::fence.failed=true;need(!vip_alert_authorize(origin,context,binding,false,error),"locked economic fence accepted");live_host_state::fence.failed=false;
    runtime.live_error="retained prior cause";
    need(!vip_alert_authorize(origin,context,binding,false,error),"retained failure ignored");
    presentation.result=false;
    vip_alert_before_deinit(42);
    need(presentation.before==1&&presentation.mode==42&&logs==1,"cleanup failure not reported");
    need(runtime.live_error=="retained prior cause","cleanup replaced original cause");
    runtime.live_error.clear();presentation.throws=true;
    vip_alert_before_deinit(42);
    need(presentation.before==2&&logs==2,"cleanup exception escaped or vanished");
    vip_alert_after_deinit(42,true);
    need(presentation.after==1&&presentation.abnormal,"abnormal original return not retained");
    need(vip_alert_initialize(error),"cleanup hook install failed");
    need(vip_alert_initialize(error),"existing hook verification failed");
    namespace hook=scenario_vip_authority::client_alert::cleanup_hook;
    need(hook::installs==1&&hook::verifies==1,"hook installed twice");
    hook::result=false;
    need(!vip_alert_initialize(error),"changed installed hook silently accepted");
    published_runtime=nullptr;
    need(!vip_alert_authorize(origin,context,binding,true,error),"missing runtime accepted");
    std::cout<<"PASS "<<checks<<" actual VIP Runtime authorization/cleanup checks\n";return 0;
}catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}}
