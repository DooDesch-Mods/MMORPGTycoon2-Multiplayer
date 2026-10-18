#include "toon_weapon_visual/authority.h"
#include "native_open_runtime.h"
#include "mt2mp/game/native_engine.h"
#include <Windows.h>
#include <atomic>
#include <iostream>
#include <stdexcept>
namespace action=mt2mp::action_session;
namespace visual=mt2mp::game::toon_weapon_visual;
namespace economy=mt2mp::game::live_host_state::day::economy;
namespace fixture {
struct Status {bool started=true,locked=false;};
struct Controller {action::Context context;action::Binding binding;Status value;
    Status status()const{return value;}const action::Context& bound_context()const{return context;}action::Binding bound_binding()const{return binding;}};
struct Session {action::Context context;bool current(action::Context& out,std::string&){out=context;return true;}};
struct Engine {bool valid=true;bool current(const action::Binding&,const std::string&,std::string& e){if(!valid)e="exact native world changed";return valid;}};
struct Bridge {economy::State state;std::optional<economy::State> applying;bool has=true,is_locked=false;bool locked()const{return is_locked;}
    const economy::State* weapon_economy()const{return applying?&*applying:committed_economy();}const economy::State* committed_economy()const{return has?&state:nullptr;}};
struct Fence {bool is_locked=false;std::string failure;bool locked()const{return is_locked;}const std::string& cause()const{return failure;}
    void journal_failed(const std::string& e){is_locked=true;failure=e;}};
Fence fence;
mt2mp::game::building_mixed_identity::opened_file::CurrentWorld world;
float scale=0;unsigned refresh_calls=0;
}
namespace placement_ui_probe {
bool ProcessMemory::copy(uintptr_t address,void* out,size_t size,std::string&){std::memcpy(out,reinterpret_cast<const void*>(address),size);return true;}
}
namespace mt2mp::game::native_open_runtime {bool read_current_world(building_mixed_identity::opened_file::CurrentWorld& out,std::string&){out=fixture::world;return true;}}
namespace mt2mp::game::native {bool read_clock(ClockState& out,std::string&){out.time_scale=fixture::scale;return true;}}
namespace mt2mp::net {uint64_t current_world_epoch(){return 7;}}
namespace mt2mp::game::live_host_state {fixture::Fence& economic_fence(){return fixture::fence;}}
namespace mt2mp::game::toon_weapon_visual {
bool initialize(uintptr_t,const Callbacks&,std::string&){return true;}
bool verify_hook(std::string&){return true;}
bool refresh(uintptr_t,Receipt& receipt,std::string&){++fixture::refresh_calls;receipt.owned=receipt.confirmed=receipt.attempted=true;return true;}
}
namespace mt2mp::game::typed_action_runtime {
struct Runtime {
    fixture::Controller controller;fixture::Session session;fixture::Engine engine;
    uint32_t thread=GetCurrentThreadId();bool lobby_retiring=false;
    std::unique_ptr<fixture::Bridge> live_state=std::make_unique<fixture::Bridge>();std::string live_error;
    toon_weapon_visual::Receipt weapon_receipt;uintptr_t weapon_toon=0;
    toon_weapon_visual::Receipt weapon_hook_receipt;uintptr_t weapon_hook_toon=0;std::optional<action::Context> weapon_bootstrap_context;action::Binding weapon_bootstrap_binding;
};
std::atomic<Runtime*> published_runtime=nullptr;
#include "toon_weapon_visual/runtime.inc"
}
namespace rt=mt2mp::game::typed_action_runtime;
namespace {
unsigned checks=0;
void need(bool okay,const char* cause){++checks;if(!okay)throw std::runtime_error(cause);}
template<class T>void put(void* object,size_t offset,T value){std::memcpy(static_cast<char*>(object)+offset,&value,sizeof value);}
struct World {
    std::unique_ptr<unsigned char[]> image=std::make_unique<unsigned char[]>(0x13df700);
    std::array<unsigned char,0x60> manager{};std::array<unsigned char,0xb0> subscriber{};std::array<unsigned char,0xa00> toon{};
    uintptr_t data=uintptr_t(subscriber.data());std::string category="swords";rt::Runtime runtime;
    World(){
        fixture::fence={};fixture::scale=0;fixture::refresh_calls=0;rt::weapon_image=uintptr_t(image.get());
        put(image.get(),0x13df690,uintptr_t(manager.data()));put(manager.data(),0,rt::weapon_image+0x1725f40);
        put(manager.data(),0x40,uintptr_t(&data));put(manager.data(),0x48,int32_t(1));put(manager.data(),0x4c,int32_t(1));
        put(subscriber.data(),0,rt::weapon_image+0x16e75f0);put(subscriber.data(),0x20,int32_t(59));put(subscriber.data(),0x80,uintptr_t(toon.data()));
        put(toon.data(),0,rt::weapon_image+0x1744e90);put(toon.data(),0x940,data);put(toon.data(),0x4b0,int32_t(12));put(toon.data(),0x4d8,int32_t(-10));
        put(toon.data(),0x4b8,uintptr_t(category.c_str()));put(toon.data(),0x4c0,uint64_t(category.size()));
        runtime.controller.context={1,2,1,3,action::Role::Client,"common"};runtime.controller.binding={2,3};runtime.session.context=runtime.controller.context;
        auto& state=runtime.live_state->state;state.day.frame.session=1;state.day.frame.snapshot="common";state.day.frame.epoch=7;
        economy::Subscriber row;row.serialized_id=59;row.quest_state.emplace();row.quest_state->weapon_ilvl=12;row.quest_state->weapon_variant_id=-10;row.quest_state->weapon_category=category;state.subscribers.push_back(row);
        fixture::world={};fixture::world.world_epoch=2;fixture::world.generation=3;fixture::world.uuid="local";fixture::world.sha256="local-sha";
        rt::published_runtime=&runtime;
    }
    bool owner(bool& owned,std::string& error){visual::Lease lease;return rt::weapon_owner(uintptr_t(toon.data()),lease,owned,error);}
};
}
int main(){try{
    {World w;bool owned=false;std::string error;need(w.owner(owned,error)&&owned,"actual callback rejected exact owner");}
    {World w;visual::Lease lease;bool owned=false;std::string error;need(rt::weapon_owner(uintptr_t(w.toon.data()),lease,owned,error)&&lease.subscriber_id==59,"actual owner lost serialized ID");
        visual::Receipt r;r.owned=r.confirmed=true;r.subscriber_id=lease.subscriber_id;rt::weapon_publish(lease.toon,r);
        auto status=rt::weapon_status(w.runtime);need(status.at("subscriber_id")==59&&status.at("toon_address")==uintptr_t(w.toon.data()),"published diagnostic receiver differs from receipt");}
    for(bool applying:{false,true}){
        World w;auto& bridge=*w.runtime.live_state;bridge.state.day.frame.revision=148;
        if(applying){bridge.applying=bridge.state;bridge.applying->day.frame.revision=149;bridge.applying->subscribers.clear();}
        else bridge.state.subscribers.clear();
        put(w.subscriber.data(),0x20,int32_t(158));
        visual::Lease lease;bool owned=false;std::string error;
        need(!rt::weapon_owner(uintptr_t(w.toon.data()),lease,owned,error)&&owned&&
            error=="Weapon authority Subscriber absent from admitted Economy","absent authority was weakened");
        const auto& d=lease.authority_failure;
        need(lease.subscriber_id==158&&d.at("subscriber_id")==158&&d.at("actual_subscriber_count")==1,"actual failed owner ID missing");
        need(d.at("selected")== (applying?"applying":"committed")&&d.at("selected_frame").at("revision")==(applying?149:148)&&
            d.at("selected_frame").at("id_matches")==0&&d.at("committed_frame").at("revision")==148,"authority phase/revision/membership differs");
        visual::Receipt r;r.owned=true;r.subscriber_id=lease.subscriber_id;r.authority_failure=d;r.cause=error;
        rt::weapon_publish(lease.toon,r);const auto status=rt::weapon_status(w.runtime);
        need(status.at("authority_failure")==d&&fixture::fence.locked()&&status.at("cause")==error,"diagnostic changed failure or lost status");
    }
    {World w;w.runtime.controller.context.role=action::Role::Host;bool owned=true;std::string error;need(w.owner(owned,error)&&!owned,"Host not passed through");}
    {World w;w.runtime.thread=0;bool owned=false;std::string error;need(!w.owner(owned,error)&&owned&&error.find("Mainthread")!=std::string::npos,"identified off-thread Client fell back");}
    {World w;w.runtime.controller.value.locked=true;bool owned=false;std::string error;need(!w.owner(owned,error)&&owned,"locked Client lost ownership");}
    {World w;w.runtime.engine.valid=false;bool owned=false;std::string error;need(!w.owner(owned,error)&&owned&&error=="exact native world changed","world cause lost");}
    {World w;fixture::scale=1;bool owned=false;std::string error;need(!w.owner(owned,error)&&owned&&error.find("Clock")!=std::string::npos,"positive Client clock accepted");}
    {World w;put(w.toon.data(),0x940,uintptr_t(0));bool owned=false;std::string error;need(!w.owner(owned,error)&&owned&&error.find("reciprocal")!=std::string::npos,"foreign Toon backlink accepted");}
    {World w;put(w.toon.data(),0x4d8,int32_t(10));bool owned=false;std::string error;need(!w.owner(owned,error)&&owned&&error.find("chosen tuple")!=std::string::npos,"local tuple not compared to authority");}
    {World w;w.runtime.live_state->has=false;bool owned=false;std::string error;need(!w.owner(owned,error)&&owned,"missing committed/admitted authority accepted");}
    {World w;visual::Receipt r;r.owned=true;r.cause="partial visual SEH";r.unknown=true;rt::weapon_publish(0,r);need(w.runtime.live_error==r.cause&&fixture::fence.locked(),"visual failure did not latch both owners");bool owned=false;std::string error;need(!w.owner(owned,error)&&owned,"failed Client retried via original");}
    {World w;rt::weapon_bootstrap(w.runtime);rt::weapon_bootstrap(w.runtime);need(fixture::refresh_calls==1&&w.runtime.weapon_bootstrap_context.has_value(),"initial visible actor refresh not exactly once per binding");w.runtime.controller.binding.native_generation=4;rt::weapon_bootstrap(w.runtime);need(fixture::refresh_calls==2,"new binding did not get initial refresh");}
    {World w;w.runtime.live_state->has=false;rt::weapon_bootstrap(w.runtime);need(!fixture::refresh_calls&&!w.runtime.weapon_bootstrap_context,"bootstrap ran before first committed Economy");}
    {World w;auto missing=w.runtime.live_state->state.subscribers.front();missing.serialized_id=60;w.runtime.live_state->state.subscribers.push_back(missing);rt::weapon_bootstrap(w.runtime);
        need(fixture::refresh_calls==1&&fixture::fence.locked()&&w.runtime.weapon_receipt.unknown&&w.runtime.weapon_receipt.attempted,"partial initial refresh lost Unknown");
        need(rt::weapon_status(w.runtime).at("unknown")==true&&!rt::weapon_status(w.runtime).at("bootstrap_confirmed").get<bool>(),"diagnostic hid partial initial refresh");}
    {World w;need(rt::weapon_status(w.runtime).at("last_owned_hook").is_null(),"unobserved hook invented");
        visual::Receipt hook;hook.owned=hook.hook=hook.confirmed=true;hook.subscriber_id=59;hook.model_id=937;hook.random.observed=hook.random.unchanged=true;
        rt::weapon_publish(uintptr_t(w.toon.data()),hook);
        visual::Receipt refresh;refresh.owned=refresh.confirmed=true;refresh.subscriber_id=60;rt::weapon_publish(1234,refresh);
        visual::Receipt foreign;foreign.hook=foreign.forwarded=true;rt::weapon_publish(9999,foreign);
        const auto status=rt::weapon_status(w.runtime);const auto& retained=status.at("last_owned_hook");
        need(status.at("subscriber_id")==60&&retained.at("subscriber_id")==59&&retained.at("toon_address")==uintptr_t(w.toon.data())&&
            retained.at("model_id")==937&&retained.at("random").at("unchanged")==true,"later refresh/unowned hook erased actual owned hook");}
    std::cout<<checks<<" actual Runtime weapon callback checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
