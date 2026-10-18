#include "toon_weapon_visual/authority.h"
#include "toon_time_fence/native.h"
#include "native_open_runtime.h"
#include "mt2mp/game/native_engine.h"
#include <Windows.h>
#include <atomic>
#include <iostream>
#include <stdexcept>
#include <type_traits>
#include <utility>
namespace action=mt2mp::action_session;
namespace visual=mt2mp::game::toon_weapon_visual;
namespace economy=mt2mp::game::live_host_state::day::economy;
static_assert(std::is_same_v<decltype(std::declval<const action::Controller&>().status()),action::Status>);
static_assert(std::is_same_v<decltype(std::declval<const action::Controller&>().bound_context()),action::Context>);
static_assert(std::is_same_v<decltype(std::declval<const action::Controller&>().status_view()),const action::Status&>);
static_assert(std::is_same_v<decltype(std::declval<const action::Controller&>().bound_context_view()),const action::Context&>);
namespace fixture {
struct Status {bool started=true,locked=false;};
struct Controller {action::Context context;action::Binding binding;Status value;
    mutable unsigned status_copies=0,context_copies=0,status_views=0,context_views=0;
    Status status()const{++status_copies;return value;}action::Context bound_context()const{++context_copies;return context;}
    const Status& status_view()const noexcept{++status_views;return value;}
    const action::Context& bound_context_view()const noexcept{++context_views;return context;}
    action::Binding bound_binding()const{return binding;}};
struct Session {action::Context context;unsigned reads=0;bool current(action::Context& out,std::string&){++reads;out=context;return true;}};
struct Engine {bool valid=true;bool current(const action::Binding&,const std::string&,std::string& e){if(!valid)e="exact native world changed";return valid;}};
struct Bridge {economy::State state;bool has=true,is_locked=false;bool locked()const{return is_locked;}
    const economy::State* weapon_economy()const{return has?&state:nullptr;}const economy::State* committed_economy()const{return weapon_economy();}};
struct Fence {bool is_locked=false;std::string failure;bool locked()const{return is_locked;}const std::string& cause()const{return failure;}
    void journal_failed(const std::string& e){is_locked=true;failure=e;}};
Fence fence;
mt2mp::game::building_mixed_identity::opened_file::CurrentWorld world;
float scale=0;unsigned refresh_calls=0,world_reads=0,clock_reads=0;bool world_ok=true;uintptr_t unreadable=0;
}
namespace placement_ui_probe {
bool ProcessMemory::copy(uintptr_t address,void* out,size_t size,std::string& error){if(address==fixture::unreadable){error="exact AP read failure";return false;}std::memcpy(out,reinterpret_cast<const void*>(address),size);return true;}
}
namespace mt2mp::game::native_open_runtime {bool read_current_world(building_mixed_identity::opened_file::CurrentWorld& out,std::string& error){if(!fixture::world_ok){error="exact world unavailable";return false;}++fixture::world_reads;out=fixture::world;return true;}}
namespace mt2mp::game::native {bool read_clock(ClockState& out,std::string&){++fixture::clock_reads;out.time_scale=fixture::scale;return true;}}
namespace mt2mp::net {uint64_t current_world_epoch(){return 7;}}
namespace mt2mp::game::live_host_state {fixture::Fence& economic_fence(){return fixture::fence;}}

namespace mt2mp::game::typed_action_runtime {
struct Runtime {
    fixture::Controller controller;fixture::Session session;
    uint32_t thread=GetCurrentThreadId();bool lobby_retiring=false;
    std::unique_ptr<fixture::Bridge> live_state=std::make_unique<fixture::Bridge>();std::string live_error;
    toon_time_fence::Receipt toon_time_receipt,toon_time_discarded;
    uintptr_t toon_time_receiver=0,toon_time_discarded_receiver=0;
    uint64_t toon_time_confirmed=0;
};
std::atomic<Runtime*> published_runtime=nullptr;
placement_ui_probe::ProcessMemory weapon_memory;
uintptr_t weapon_image=0x140000000;
#include "toon_time_fence/runtime.inc"
}
namespace rt=mt2mp::game::typed_action_runtime;
namespace tf=mt2mp::game::toon_time_fence;
namespace {
unsigned checks=0;
void need(bool okay,const char* cause){++checks;if(!okay)throw std::runtime_error(cause);}
template<class T>void put(void* object,size_t offset,T value){std::memcpy(static_cast<char*>(object)+offset,&value,sizeof value);}
struct World {
    std::array<unsigned char,0xb0> subscriber{};std::array<unsigned char,0xb00> toon{};rt::Runtime runtime;
    World(){
        fixture::fence={};fixture::world_reads=fixture::clock_reads=0;fixture::scale=0;fixture::world_ok=true;fixture::unreadable=0;
        put(subscriber.data(),0,rt::weapon_image+0x16e75f0);put(subscriber.data(),0x20,int32_t(60));put(subscriber.data(),0x80,uintptr_t(toon.data()));
        put(toon.data(),0,rt::weapon_image+0x1744e90);put(toon.data(),0x940,uintptr_t(subscriber.data()));put(toon.data(),0x170,uintptr_t(0xabc000+0x1c0));
        runtime.controller.context={1,2,1,3,action::Role::Client,"common"};runtime.controller.binding={2,3};runtime.session.context=runtime.controller.context;
        auto& state=runtime.live_state->state;state.day.frame.session=1;state.day.frame.snapshot="common";state.day.frame.epoch=7;
        fixture::world={};fixture::world.world_epoch=2;fixture::world.generation=3;fixture::world.uuid="local";fixture::world.sha256="local-sha";fixture::world.map=reinterpret_cast<void*>(0xabc000);
        rt::published_runtime=&runtime;
    }
    bool owner(bool& owned,std::string& error){tf::Owner out;return rt::toon_time_owner(uintptr_t(toon.data()),out,owned,error);}
};
}

struct OwnerCalls {
    using Owner=tf::Owner;
    unsigned effects=0;
    bool pin(uintptr_t toon,Owner& out,bool& owned,std::string& error){return rt::toon_time_owner(toon,out,owned,error);}
    bool read_backlog(uintptr_t,uint32_t&,std::string&){++effects;return false;}
    bool clear_backlog(uintptr_t,uint32_t,std::string&){++effects;return false;}
    bool original(uintptr_t,float,std::string&){++effects;return false;}
};
struct FreshOwnerCalls:OwnerCalls {
    unsigned pins=0,backlog_reads=0,originals=0,clears=0,change_pin=0;
    bool change_membership=false;
    uint32_t bits=std::bit_cast<uint32_t>(2.0f);
    bool pin(uintptr_t toon,Owner& out,bool& owned,std::string& error){
        ++pins;
        if(pins==change_pin){
            if(change_membership)rt::published_runtime.load()->session.context.peer_mask=7;
            else fixture::world.sha256="changed-local-sha";
        }
        return OwnerCalls::pin(toon,out,owned,error);
    }
    bool read_backlog(uintptr_t,uint32_t& out,std::string&){++backlog_reads;out=bits;return true;}
    bool clear_backlog(uintptr_t,uint32_t expected,std::string&){++clears;if(bits!=expected)return false;bits=0;return true;}
    bool original(uintptr_t,float dt,std::string&){++originals;return dt==0.0f&&bits==0;}
};
void fresh_owner_checks(){
    {
        World w;FreshOwnerCalls calls;tf::Receipt receipt;
        need(rt::toon_time_client_active(),"fresh active Client rejected");
        need(tf::normalize(calls,uintptr_t(w.toon.data()),3.0f,receipt)&&receipt.confirmed&&receipt.cache_cleared,
            "fresh actual owner normalization rejected");
        const auto& controller=w.runtime.controller;
        need(calls.pins==3&&calls.backlog_reads==3&&calls.clears==1&&calls.originals==1&&calls.bits==0,
            "owner copy change lost pin/backlog/CAS/original boundaries");
        need(w.runtime.session.reads==3&&fixture::world_reads==3&&fixture::clock_reads==3,
            "owner copy change reused Session/world/Clock proof");
        need(controller.status_views==7&&controller.context_views==7&&!controller.status_copies&&!controller.context_copies,
            "Toon active/owner checks constructed copying Controller temporaries");
        tf::Owner out;bool owned=false;std::string error;
        need(rt::toon_time_owner(uintptr_t(w.toon.data()),out,owned,error)&&owned&&
            out.origin_uuid==fixture::world.uuid&&out.origin_sha==fixture::world.sha256&&
            fixture::world.uuid=="local"&&fixture::world.sha256=="local-sha",
            "moving local NativeOpen strings changed provider state or owner receipt");
    }
    for(unsigned pin:{2u,3u})for(bool membership:{false,true}){
        World w;FreshOwnerCalls calls;tf::Receipt receipt;calls.change_pin=pin;calls.change_membership=membership;
        need(!tf::normalize(calls,uintptr_t(w.toon.data()),0.0f,receipt)&&!receipt.confirmed&&!receipt.forward&&
            receipt.unknown==(pin==3)&&calls.originals==(pin==3?1u:0u)&&calls.pins==pin,
            "fresh second/final owner or Session membership change accepted");
        need(receipt.cause==(membership?"Toon time current Session differs from Controller":
            pin==2?"Toon time owner changed before cache normalization":"Toon time owner changed after original entry"),
            "fresh owner change lost exact boundary cause");
    }
}
void ap_read_failure(){
    World w;OwnerCalls calls;tf::Receipt receipt;
    const auto toon=uintptr_t(w.toon.data());fixture::unreadable=toon;
    const bool result=tf::normalize(calls,toon,3.0f,receipt);
    rt::toon_time_publish(toon,receipt);
    need(!result&&receipt.owned&&!receipt.forward&&!receipt.unknown&&!calls.effects&&
        receipt.cause=="exact AP read failure"&&w.runtime.live_error==receipt.cause&&
        fixture::fence.locked()&&fixture::fence.cause()==receipt.cause,
        "unreadable active Client receiver silently suppressed original/read cause");
}

int main(int argc,char**){try{
    ap_read_failure();
    if(argc>1){std::cout<<checks<<" actual unreadable receiver check passed\n";return 0;}
    fresh_owner_checks();
    {World w;bool owned=false;std::string error;need(rt::toon_time_client_active()&&w.owner(owned,error)&&owned,"exact owner rejected");}
    {World w;tf::Owner out;bool owned=false;std::string error;need(rt::toon_time_owner(uintptr_t(w.toon.data()),out,owned,error)&&out.subscriber_id==60&&out.toon==uintptr_t(w.toon.data())&&out.origin[4]==0xabc000,"owner receipt identity differs");}
    {World w;w.runtime.controller.context.role=action::Role::Host;bool owned=true;std::string error;need(!rt::toon_time_client_active()&&w.owner(owned,error)&&!owned,"Host not passed through");}
    {World w;w.runtime.controller.value.started=false;bool owned=true;std::string error;need(!rt::toon_time_client_active()&&w.owner(owned,error)&&!owned,"unjoined not passed through");}
    {World w;w.runtime.live_state->has=false;bool owned=true;std::string error;need(!rt::toon_time_client_active()&&w.owner(owned,error)&&!owned,"pre-authority bootstrap not passed through");}
    {World w;w.runtime.thread=0;bool owned=false;std::string error;need(!w.owner(owned,error)&&owned&&error.find("Mainthread")!=std::string::npos,"owned wrong thread fell through");}
    {World w;w.runtime.controller.value.locked=true;bool owned=false;std::string error;need(!w.owner(owned,error)&&owned,"locked owner fell through");}
    {World w;w.runtime.controller.value.locked=true;put(w.toon.data(),0,rt::weapon_image+0x111);bool owned=true;std::string error;need(w.owner(owned,error)&&!owned,"unrelated class claimed during Client lock");}
    {World w;fixture::world_ok=false;bool owned=false;std::string error;need(!w.owner(owned,error)&&owned&&error=="exact world unavailable","NativeOpen cause lost");}
    {World w;fixture::world.generation=9;bool owned=false;std::string error;need(!w.owner(owned,error)&&owned&&error.find("lifetime")!=std::string::npos,"different NativeOpen binding accepted");}
    {World w;w.runtime.session.context.session=2;bool owned=false;std::string error;need(!w.owner(owned,error)&&owned&&error.find("Session")!=std::string::npos,"different Session accepted");}
    {World w;w.runtime.live_state->state.day.frame.snapshot="other";bool owned=false;std::string error;need(!w.owner(owned,error)&&owned&&error.find("Economy")!=std::string::npos,"different authority context accepted");}
    {World w;fixture::scale=1;bool owned=false;std::string error;need(!w.owner(owned,error)&&owned&&error.find("Clock")!=std::string::npos,"positive Client clock accepted");}
    {World w;put(w.subscriber.data(),0x80,uintptr_t(0));bool owned=false;std::string error;need(!w.owner(owned,error)&&owned&&error.find("reciprocal")!=std::string::npos,"broken reciprocal owner accepted");}
    {World w;put(w.subscriber.data(),0x20,int32_t(-1));bool owned=false;std::string error;need(!w.owner(owned,error)&&owned,"invalid serialized ID accepted");}
    {World w;put(w.toon.data(),0x170,uintptr_t(0));bool owned=false;std::string error;need(!w.owner(owned,error)&&owned&&error.find("root shard")!=std::string::npos,"foreign shard accepted");}
    {World w;tf::Receipt r;r.owned=r.confirmed=r.original_returned=true;r.subscriber_id=60;r.backlog_bits=std::bit_cast<uint32_t>(2.0f);rt::toon_time_publish(uintptr_t(w.toon.data()),r);
        r.subscriber_id=61;r.backlog_bits=0;rt::toon_time_publish(1234,r);auto status=rt::toon_time_status(w.runtime);
        need(status.at("confirmed_updates")==2&&status.at("last").at("subscriber_id")==61&&status.at("last_discarded").at("subscriber_id")==60,"last discarded backlog erased by later zero call");}
    {World w;tf::Receipt r;r.owned=true;r.unknown=true;r.cause="exact partial failure";rt::toon_time_publish(1,r);r.cause="later locked owner";rt::toon_time_publish(1,r);
        need(w.runtime.live_error=="exact partial failure"&&fixture::fence.cause()=="exact partial failure","sticky original cause lost");bool owned=false;std::string error;need(!w.owner(owned,error)&&owned,"faulted Client could retry original");}
    {World w;tf::Receipt r;r.cause="foreign";rt::toon_time_publish(1,r);need(w.runtime.live_error.empty()&&!fixture::fence.locked(),"unowned receipt latched failure");}
    std::cout<<checks<<" actual Runtime time owner checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
