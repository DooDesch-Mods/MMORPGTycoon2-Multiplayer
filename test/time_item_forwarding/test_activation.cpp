#include "controller_flow.h"
#include "../../src/game/hud_time_activation.h"
#include <cstdio>
using namespace time_mouse_tests;
namespace ui=mt2mp::game::shared_time;
namespace {
int checks=0,failures=0;
void check(bool value,const char* label){++checks;if(!value){++failures;std::fprintf(stderr,"FAIL %s\n",label);}}
void required(bool value,const std::string& cause){if(!value)throw std::runtime_error(cause);}
struct HookFixture {
    ControllerPeer peer;
    hud::Reader reader;
    hud::Activation activation;
    hud::ActivationContext context;
    int contexts=0,censuses=0,originals=0;
    std::function<void(int)> context_change;
    bool context_fails=false,context_throws=false,submit_throws=false,thread_throws=false;
    explicit HookFixture(bool host=false):peer(host),context{peer.epoch,uint8_t(host?1:2),3,host,true,3}{
        std::string cause;required(reader.initialize(peer.memory.config(),peer.memory.services(),cause),cause);
        hud::ActivationServices services;
        services.client=!host;
        services.thread_id=[&](){if(thread_throws)throw std::runtime_error("fixture original thread callback failure");return peer.memory.thread;};
        services.context=[&](hud::ActivationContext& out,std::string& error){++contexts;if(context_change)context_change(contexts);
            if(context_throws)throw std::runtime_error("fixture original authority callback failure");
            if(context_fails){error="fixture original authority read failure";return false;}out=context;return true;};
        services.submit=[&](wire::Intent intent){if(submit_throws)throw std::runtime_error("fixture original submit failure");return peer.submit(intent);};
        required(activation.initialize(17,reader,std::move(services),cause),cause);
        peer.memory.callback=[&](uintptr_t at){if(at==image+0x13df8f0)++censuses;};
    }
    ui::HookResult invoke(size_t index=3,uintptr_t caller=hud::Activation::mouse_caller_rva){return activation.on_activate(NativeReadFixture::type(index),NativeReadFixture::item(index),caller);}
    ui::HookResult dispatch(size_t index=3){auto result=invoke(index);if(result.decision==ui::Decision::Forward){++originals;peer.memory.put(time_mouse_tests::clock+0x58,scales[index]);}return result;}
};
void forwarded_guard_regression(){
    {HookFixture fixture;fixture.peer.memory.put(NativeReadFixture::item_proxy(3),NativeReadFixture::item(3)+0x18);auto result=fixture.dispatch();
        check(result.decision==ui::Decision::Suppress&&!result.error.empty()&&fixture.originals==0&&fixture.peer.memory.get<float>(time_mouse_tests::clock+0x58)==0,"known Mouse Reader weak failure must not forward native client Clock9");}
    {HookFixture fixture;fixture.peer.memory.thread=18;auto result=fixture.invoke();
        check(result.decision==ui::Decision::Forward&&result.error=="hud_activation_wrong_thread"&&fixture.contexts==0&&fixture.censuses==0,"worker stays outside proven native Mainthread UI route");}
    {HookFixture fixture;fixture.peer.memory.throwing=true;auto result=fixture.dispatch();fixture.peer.memory.throwing=false;
        check(result.decision==ui::Decision::Suppress&&result.error.find("fixture original read failure")!=std::string::npos&&fixture.originals==0,"known Mouse original Reader exception must not forward native Original");}
    {HookFixture fixture;fixture.thread_throws=true;auto result=fixture.invoke();
        check(result.error.find("fixture original thread callback failure")!=std::string::npos&&fixture.contexts==0&&fixture.censuses==0,"thread callback exception preserves cause before native thread classification");}
    {HookFixture fixture;ui::HookResult nested;fixture.context_change=[&](int count){if(count==1)nested=fixture.dispatch();};auto result=fixture.invoke();
        check(nested.decision==ui::Decision::Suppress&&nested.error=="hud_activation_reentrant"&&fixture.originals==0,"known Mouse reentry must not forward native Original");
        check(result.decision==ui::Decision::Suppress,"outer activation remains suppressed after reentry");}
}
void successful_client_flow(){
    ControllerPeer host(true);HookFixture client;initial_host_state(host,client.peer);client.censuses=0;
    auto result=client.dispatch();check(result.matched&&result.origin_valid&&result.origin==ui::Origin::Mouse&&result.intent==ui::Intent::Fastest&&result.decision==ui::Decision::Suppress&&result.request_queued,"HUD clone Mouse enters actual Controller");
    check(client.censuses==4&&client.contexts==2&&client.peer.submissions==1&&client.originals==0&&host.native_sets==0&&client.peer.native_sets==0&&client.peer.memory.get<float>(time_mouse_tests::clock+0x58)==0,"four reader censuses and two contexts before submit; no Clock effect before Host authority");
    auto requests=drain(client.peer);required(requests.size()==1&&requests[0].message.kind==wire::Kind::Request,"actual client request missing");auto packet=requests[0].message;
    auto applied=host.controller.receive(2,packet);check(applied.okay()&&applied.effect_confirmed&&host.native_sets==1&&host.memory.get<float>(time_mouse_tests::clock+0x58)==9,"actual Host Controller applies requested scale once");
    auto duplicate=host.controller.receive(2,serialized(packet));check(duplicate.okay()&&duplicate.disposition==wire::Disposition::Duplicate&&host.native_sets==1,"serialized duplicate request has no second Host Clock setter");
    for(const auto& outbound:drain(host)){auto ack=client.peer.controller.receive(1,outbound.message);check(ack.okay(),"actual Host state/receipt accepted by Client");}
    for(const auto& outbound:drain(client.peer)){auto ack=host.controller.receive(2,outbound.message);check(ack.okay(),"actual Client presentation ACK accepted by Host");}
    check(client.peer.memory.get<float>(time_mouse_tests::clock+0x58)==0&&client.peer.native_sets==0&&client.peer.presentations==2,"Client native Clock stays frozen after authoritative scale9 state");
}
void negative_adapter_guards(){
    {HookFixture fixture;auto r=fixture.activation.on_activate(NativeReadFixture::type(3),NativeReadFixture::inventory_item(3),hud::Activation::mouse_caller_rva);check(!r.matched&&r.decision==ui::Decision::Suppress&&r.error=="hud_activation_mouse_pair_not_owned"&&fixture.peer.submissions==0,"Client InventoryPair rejected visibly without authority request");}
    {HookFixture fixture;auto r=fixture.activation.on_activate(NativeReadFixture::type(2),NativeReadFixture::item(3),hud::Activation::mouse_caller_rva);check(!r.matched&&r.decision==ui::Decision::Suppress&&r.error=="hud_activation_mouse_pair_not_owned"&&fixture.peer.submissions==0,"Client foreign type pair stays suppressed without request");}
    {HookFixture fixture(true);auto r=fixture.activation.on_activate(NativeReadFixture::type(3),NativeReadFixture::inventory_item(3),hud::Activation::mouse_caller_rva);check(!r.matched&&r.decision==ui::Decision::Forward&&fixture.peer.submissions==0,"Host foreign mouse pair passes Original");}
    {HookFixture fixture;auto r=fixture.invoke(3,0x1234);check(!r.matched&&r.decision==ui::Decision::Forward&&fixture.censuses==0&&fixture.peer.submissions==0,"foreign caller bypasses capture without native census");}
    {HookFixture fixture;fixture.peer.memory.put(panel+0x45f,uint8_t(0));auto r=fixture.dispatch();check(r.matched&&r.decision==ui::Decision::Suppress&&!r.request_queued&&r.error=="hud_activation_button_not_available"&&fixture.originals==0,"hidden Clockpanel rejects without native Original");}
    {HookFixture fixture;fixture.peer.memory.put(NativeReadFixture::button(3)+0x381,uint8_t(0));auto r=fixture.dispatch();check(r.matched&&r.error=="hud_activation_button_not_available"&&fixture.originals==0,"disabled button rejects without native Original");}
    for(int change=0;change<6;++change){HookFixture fixture;switch(change){case 0:fixture.context.active=false;break;case 1:fixture.context.epoch=0;break;case 2:fixture.context.hook_mask=1;break;case 3:fixture.context.peer_mask=1;break;case 4:fixture.context.host=true;break;default:fixture.context.local_peer=0;}
        auto r=fixture.dispatch();check(r.decision==ui::Decision::Suppress&&r.error==(change==4?"hud_activation_role_changed":"hud_activation_authority_not_ready")&&fixture.peer.submissions==0&&fixture.originals==0,"actual authority guard prevents local native effect");}
    {HookFixture fixture;fixture.context_change=[&](int count){if(count==2)++fixture.context.epoch;};auto r=fixture.dispatch();check(r.error=="hud_activation_authority_changed"&&fixture.peer.submissions==0&&fixture.originals==0,"authority epoch change between observations blocks submit");}
    {HookFixture fixture;fixture.peer.memory.callback=[&](uintptr_t at){if(at==image+0x13df8f0&&++fixture.censuses==3)fixture.peer.memory.put(mode+0x58,world+0x20);};auto r=fixture.dispatch();check(r.error=="hud_activation_binding_changed"&&fixture.peer.submissions==0&&fixture.originals==0,"actual native world changes between classifications block submit");}
    {HookFixture fixture;fixture.peer.memory.callback=[&](uintptr_t at){if(at==image+0x13df8f0&&++fixture.censuses==3){auto replacement=NativeReadFixture::item_proxy(3)+0x200;fixture.peer.memory.put(replacement,NativeReadFixture::type(3)+0x38);fixture.peer.memory.put(NativeReadFixture::item(3)+0x70,replacement);fixture.peer.memory.put(NativeReadFixture::type(3)+0x40,replacement);}};
        auto r=fixture.dispatch();check(r.error=="hud_activation_binding_changed"&&fixture.peer.submissions==0&&fixture.originals==0,"actual type proxy replacement between classifications blocks submit");}
    {HookFixture fixture;fixture.peer.memory.callback=[&](uintptr_t at){if(at==image+0x13df8f0&&++fixture.censuses==3)fixture.peer.memory.put(NativeReadFixture::item(3)+0x38,int32_t(0));};auto r=fixture.dispatch();check(r.error=="hud_activation_binding_changed"&&fixture.peer.submissions==0&&fixture.originals==0,"native item state change between classifications blocks submit");}
    {HookFixture fixture;fixture.peer.memory.put(time_mouse_tests::clock+0x58,1.f);auto r=fixture.invoke();check(r.error=="hud_activation_client_clock_not_zero"&&fixture.peer.submissions==0&&r.decision==ui::Decision::Suppress,"nonfrozen actual Client Clock is rejected");}
    {HookFixture fixture;fixture.context_fails=true;auto r=fixture.dispatch();check(r.error.find("fixture original authority read failure")!=std::string::npos&&fixture.originals==0,"authority failure retains original cause");}
    {HookFixture fixture;fixture.context_throws=true;auto r=fixture.dispatch();check(r.error.find("fixture original authority callback failure")!=std::string::npos&&fixture.originals==0,"authority exception retains original cause");}
    {HookFixture fixture;fixture.submit_throws=true;auto r=fixture.dispatch();check(r.error.find("fixture original submit failure")!=std::string::npos&&fixture.originals==0&&!r.request_queued,"submit exception visible without Original fallback");}
    {HookFixture fixture;auto r=fixture.dispatch();check(r.error.find("hud_activation_submit_failed")!=std::string::npos&&!r.request_queued&&fixture.originals==0&&fixture.peer.native_sets==0,"actual Controller NoState failure is visible and does not apply native Clock");}
    {HookFixture fixture(true);auto result=fixture.dispatch();check(result.request_queued&&result.decision==ui::Decision::Suppress&&fixture.peer.submissions==1&&fixture.peer.native_sets==1&&fixture.originals==0&&fixture.peer.memory.get<float>(time_mouse_tests::clock+0x58)==9,"Host Mouse uses same actual Controller with one authorized Clock effect");}
    {HookFixture fixture(true);fixture.peer.set_unknown=true;auto first=fixture.dispatch();wire::Status status;auto report=fixture.peer.controller.status(status);
        check(first.decision==ui::Decision::Suppress&&!first.request_queued&&first.error.find("fixture original native setter unknown")!=std::string::npos&&report.okay()&&status.write_locked&&fixture.peer.native_sets==1&&fixture.originals==0,"Host native setter Unknown retains cause and actual Controller write lock");
        auto second=fixture.dispatch();check(second.decision==ui::Decision::Suppress&&!second.request_queued&&fixture.peer.native_sets==1&&fixture.originals==0,"manual second intent after Unknown cannot replay native setter");}
}
}
int main(){try{forwarded_guard_regression();successful_client_flow();negative_adapter_guards();std::printf("%s Activation %d checks failures=%d; native game acceptance false\n",failures?"FAIL":"PASS",checks,failures);return failures?1:0;}catch(const std::exception& cause){std::fprintf(stderr,"error operation=time_mouse_activation cause=%s\n",cause.what());return 1;}}
