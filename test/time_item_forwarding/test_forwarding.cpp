#include "item_activation_origin.h"
#include <thread>
#define main legacy_time_activation_main
#include "test_activation.cpp"
#undef main
namespace mt2mp::game::time_integration {
struct Adapter {shared_time::HookResult on_activate(void*,void*,uintptr_t){return {};}};
Adapter& instance(){static Adapter value;return value;}
}
namespace mt2mp::game::time_input_diagnostics {
void leaf(uintptr_t,const shared_time::HookResult&,uint64_t){}
}
namespace {
namespace game=mt2mp::game;
namespace origin=game::item_activation_origin;
HookFixture* forwarding_fixture=nullptr;
uintptr_t native_image_begin=image,native_image_end=image+0x2000000,physical_return=image+0x4000000;
uintptr_t time_last_caller_rva=0;
uint64_t time_activate_attempted=0,time_activate_matched=0,time_activate_suppressed=0,time_activate_original=0,completed_game_updates=0;
game::shared_time::Intent time_last_intent=game::shared_time::Intent::Pause;
bool time_last_request_queued=false,owned_native_profile=true;
std::string time_mouse_error;uint64_t time_mouse_error_repeats=0;
void* fixture_return_address(){return reinterpret_cast<void*>(physical_return);}
void original_time_activate(void*,void*){++forwarding_fixture->originals;forwarding_fixture->peer.memory.put(time_mouse_tests::clock+0x58,3.f);}
#define hud_time_activation forwarding_fixture->activation
#define LOG_ERROR(...) ((void)0)
#define LOG_INFO(...) ((void)0)
#include "time_item_hook.inc"
#undef LOG_INFO
#undef LOG_ERROR
#undef hud_time_activation
void original_item(void* item){time_activate(reinterpret_cast<void*>(NativeReadFixture::type(2)),item);}
void throwing_item(void*){throw std::runtime_error("original forwarded Item exception");}
uintptr_t resolve(uintptr_t item){return origin::caller(image,native_image_end,item,physical_return);}
void nested_foreign(void* item){
    check(resolve(uintptr_t(item))==image+0x1234,"nested caller uses its own native origin");
    check(resolve(NativeReadFixture::item(2))==0,"nested different Item cannot inherit outer origin");
}
void nested_deactivate(void* item){check(resolve(uintptr_t(item))==0,"nested Deactivate masks outer Activate origin");}
void outer_nested(void* item){
    const auto current=uintptr_t(item);
    check(resolve(current)==image+hud::Activation::mouse_caller_rva,"outer original native caller available");
    origin::forward(nested_foreign,reinterpret_cast<void*>(NativeReadFixture::item(3)),image,image+0x1234,true);
    check(resolve(current)==image+hud::Activation::mouse_caller_rva,"outer caller restored after nested Item return");
    origin::forward(nested_deactivate,item,image,image+hud::Activation::mouse_caller_rva,false);
    check(resolve(current)==image+hud::Activation::mouse_caller_rva,"outer caller restored after nested Deactivate");
    check(resolve(current+8)==0,"unrelated Item cannot use active origin");
    check(origin::caller(image+1,native_image_end,current,physical_return)==0,"different image cannot use active origin");
    check(origin::caller(image,native_image_end,current,image+0x1111)==image+0x1111,"genuine native caller is never replaced by carried origin");
    uintptr_t worker=1;std::thread thread([&](){worker=resolve(current);});thread.join();check(worker==0,"worker cannot inherit Mainthread forwarding origin");
}
void forwarded_chain(){
    const auto item=reinterpret_cast<void*>(NativeReadFixture::item(2));
    {
        HookFixture baseline;forwarding_fixture=&baseline;
        original_item(item);
        check(time_last_caller_rva==0&&baseline.activation.status().attempted==0&&baseline.originals==1&&baseline.peer.memory.get<float>(time_mouse_tests::clock+0x58)==3,"original wrapped tail-dispatch reproduces unadmitted Client Clock3");
    }
    {
        ControllerPeer host(true);HookFixture client;forwarding_fixture=&client;initial_host_state(host,client.peer);
        const auto before=time_activate_original;
        origin::forward(original_item,item,image,image+hud::Activation::mouse_caller_rva,true);
        check(time_last_caller_rva==hud::Activation::mouse_caller_rva&&client.activation.status().submitted==1&&time_activate_original==before&&client.originals==0,"carried same-Item native origin reaches actual Time hook and Controller exactly once");
        check(client.peer.memory.get<float>(time_mouse_tests::clock+0x58)==0,"Client clock remains zero after native Item returns");
        auto requests=drain(client.peer);required(requests.size()==1,"forwarded Time request missing");
        const auto applied=host.controller.receive(2,requests[0].message);
        check(applied.okay()&&host.native_sets==1&&host.memory.get<float>(time_mouse_tests::clock+0x58)==3,"forwarded Time request reaches Host Clock3 once");
        for(const auto& row:drain(host))check(client.peer.controller.receive(1,row.message).okay(),"Client accepts Host authoritative Time receipt");
        for(const auto& row:drain(client.peer))check(host.controller.receive(2,row.message).okay(),"Host accepts Client authoritative Time ACK");
        check(client.peer.memory.get<float>(time_mouse_tests::clock+0x58)==0&&client.peer.native_sets==0,"Client Time ACK has no local simulation setter");
        check(resolve(uintptr_t(item))==0,"returned forwarding scope cannot authorize a later call");
    }
    origin::forward(outer_nested,item,image,image+hud::Activation::mouse_caller_rva,true);
    check(resolve(uintptr_t(item))==0,"nested forwarding leaves no stale origin");
    try{origin::forward(throwing_item,item,image,image+hud::Activation::mouse_caller_rva,true);check(false,"original exception swallowed");}
    catch(const std::runtime_error& error){check(std::string(error.what())=="original forwarded Item exception","original forwarding exception retains cause");}
    check(resolve(uintptr_t(item))==0,"exception exit leaves no stale origin");
    {
        HookFixture foreign;forwarding_fixture=&foreign;
        origin::forward(original_item,item,image,image+0x1234,true);
        check(time_last_caller_rva==0x1234&&foreign.activation.status().attempted==0&&foreign.peer.submissions==0,"unrelated native caller is not promoted to HUD Mouse");
    }
    {
        ControllerPeer host(true);HookFixture reentrant;forwarding_fixture=&reentrant;initial_host_state(host,reentrant.peer);
        reentrant.context_change=[&](int count){if(count==1)origin::forward(original_item,item,image,image+hud::Activation::mouse_caller_rva,true);};
        origin::forward(original_item,item,image,image+hud::Activation::mouse_caller_rva,true);
        check(reentrant.activation.status().rejected==1&&reentrant.activation.status().submitted==1&&reentrant.originals==0,"same-Item reentry reaches existing Time rejection without Original fallback");
        check(resolve(uintptr_t(item))==0,"reentry leaves no carried context");
    }
}
}
int main(){const auto previous=legacy_time_activation_main();if(previous)return previous;
    try{forwarded_chain();std::printf("%s Time Item forwarding %d total checks failures=%d\n",failures?"FAIL":"PASS",checks,failures);return failures?1:0;}
    catch(const std::exception& error){std::fprintf(stderr,"Time Item forwarding failure: %s\n",error.what());return 1;}}
