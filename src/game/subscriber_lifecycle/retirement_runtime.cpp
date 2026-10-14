#include "retirement_runtime.h"
#include "client_world.h"
#include "append_calls.h"
#include "native_open_runtime.h"
#include "session_provider.h"
#include "mt2mp/net/snapshot_sync.h"
#include <Windows.h>
#include <algorithm>
#include <array>
#include <map>
#include <stdexcept>
namespace mt2mp::game::subscriber_retirement {
namespace {
namespace local=mt2mp::subscriber_lifecycle;
namespace first=local::first_append;
using Json=nlohmann::json;
uintptr_t image=0;live_host_state::day::economy::Owners bound;
std::map<int32_t,int32_t> retired;
std::string blocked;
bool active=false,client_permit=false;
Json latest;
class Memory final:public local::Memory {
public:bool read(uintptr_t p,void* out,size_t size,std::string& e)override{
    SIZE_T copied=0;if(!p||p>UINTPTR_MAX-size||!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(p),out,size,&copied)||copied!=size){e="Subscriber retirement native read failed Win32="+std::to_string(GetLastError());return false;}return true;
}} memory;
bool fail(std::string& e,const std::string& cause){e=cause;return false;}
template<class T>bool at(uintptr_t p,size_t offset,T& value,std::string& e){if(!p||p>UINTPTR_MAX-offset)return fail(e,"Subscriber retirement native field extent differs");return memory.read(p+offset,&value,sizeof(value),e);}
bool current(first::ClientWorld& world,std::string& e){
    const auto& p=bound.history.pin;action_session::Context context;
    if(!image||!p.world||!action_session::selected_session().current(context,e)||context!=p.context||GetCurrentThreadId()!=p.thread||net::current_world_epoch()!=p.common_epoch)return fail(e,e.empty()?"Subscriber retirement current session/Mainthread differs":e);
    world={};world.image=image;world.thread=p.thread;world.manager=bound.subscriber_manager;
    if(!native_open_runtime::read_current_world(world.origin,e))return false;
    if(uintptr_t(world.origin.world)!=p.world||world.origin.world_epoch!=p.binding.native_epoch||world.origin.generation!=p.binding.native_generation)return fail(e,"Subscriber retirement actual World/epoch/generation differs");
    uintptr_t manager=0,type=0;
    if(!at(image,0x13df690,manager,e)||manager!=world.manager||!at(manager,0,type,e)||type!=image+0x1725f40)return fail(e,e.empty()?"Subscriber retirement actual Manager/AP differs":e);
    return true;
}
bool prepare(first::ClientWorld& world,uintptr_t manager,uintptr_t subscriber,local::Census& before,int32_t& id,int32_t& reason,uint64_t& uid,uint8_t& dispatch,std::string& e){
    if(!current(world,e)||manager!=world.manager||!first::read_cohort(memory,world,before,e))return false;
    auto found=std::find_if(before.members.begin(),before.members.end(),[&](const auto& row){return row.subscriber==subscriber;});
    if(found==before.members.end()||found->online||retired.contains(found->id))return fail(e,"Subscriber retirement receiver absent/online/already retired");
    id=found->id;uintptr_t actor=0,registered=0;uint8_t logging_out=1,queued=1;
    if(!at(subscriber,0x120,reason,e)||reason<0||!at(image,0x13b9820,dispatch,e)||dispatch>1)return fail(e,e.empty()?"Subscriber retirement original reason/EventManager flag unreadable":e);
    if(!found->toon){uid=0;return true;}
    if(!at(found->toon,0x18,uid,e)||!uid||!at(found->toon,0x6f0,actor,e)||actor||!at(found->toon,0x9ed,logging_out,e)||logging_out||!at(found->toon,0x9ec,queued,e)||queued)return fail(e,e.empty()?"Subscriber retirement requires actual offline Toon/actor/queues":e);
    if(!first::registry_lookup(image,uid,registered,e)||registered!=found->toon)return fail(e,e.empty()?"Subscriber retirement actual Toon registry owner differs":e);
    return true;
}
bool confirm(const first::ClientWorld& world,const local::Census& before,uintptr_t subscriber,int32_t id,uint64_t uid,std::string& e){
    first::ClientWorld now;if(!current(now,e)||now.manager!=world.manager)return false;
    local::Census after;if(!first::read_cohort(memory,now,after,e))return false;
    if(after.next_id!=before.next_id||before.members.size()!=after.members.size()+1)return fail(e,"Subscriber original Manager retirement count/nextID differs");
    auto expected=before.members;auto found=std::find_if(expected.begin(),expected.end(),[&](const auto& row){return row.id==id&&row.subscriber==subscriber;});
    if(found==expected.end())return fail(e,"Subscriber original Manager retirement prior ID absent");
    *found=expected.back();expected.pop_back();
    if(after.members!=expected)return fail(e,"Subscriber original Manager swap-with-last survivor census differs");
    for(const auto& row:after.members){uintptr_t data=0;int32_t count=-1,capacity=-1;
        if(!at(row.subscriber,0xf0,data,e)||!at(row.subscriber,0xf8,count,e)||!at(row.subscriber,0xfc,capacity,e)||count<0||capacity<count||size_t(count)>after.members.size())return fail(e,e.empty()?"Subscriber retirement friend array bounds differ":e);
        for(int32_t i=0;i<count;++i){uintptr_t friend_owner=0;if(!at(data,size_t(i)*8,friend_owner,e))return false;if(friend_owner==subscriber)return fail(e,"Subscriber original D1 left a reciprocal friend reference");}
    }
    uintptr_t actual=0;if(uid&&(!first::registry_lookup(image,uid,actual,e)||actual))return fail(e,e.empty()?"Subscriber original D0 left Toon UID registered":e);
    return true;
}
uint32_t unsubscribe(uintptr_t receiver,int32_t reason){__try{reinterpret_cast<void(*)(uintptr_t,int32_t)>(image+0x2ea830)(receiver,reason);return 0;}__except(GetExceptionCode()==0xe06d7363?EXCEPTION_CONTINUE_SEARCH:EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
uint32_t remove(uintptr_t manager,uintptr_t subscriber){__try{reinterpret_cast<Remove>(image+0x2ef4c0)(reinterpret_cast<void*>(manager),reinterpret_cast<void*>(subscriber));return 0;}__except(GetExceptionCode()==0xe06d7363?EXCEPTION_CONTINUE_SEARCH:EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
}
bool initialize(uintptr_t module,std::string& e){
    struct Code{uintptr_t rva;std::array<unsigned char,16> bytes;};
    static constexpr Code codes[]={
        {0x2ef4c0,{0x41,0x54,0x55,0x57,0x56,0x53,0x48,0x81,0xec,0x20,0x02,0x00,0x00,0x0f,0x11,0xb4}},
        {0x2ea830,{0x57,0x56,0x53,0x48,0x81,0xec,0xd0,0x01,0x00,0x00,0xc6,0x81,0x90,0x00,0x00,0x00}},
        {0x2ec2f0,{0x53,0x48,0x83,0xec,0x20,0x48,0x89,0xcb,0xe8,0xa3,0xfd,0xff,0xff,0xba,0x28,0x01}},
        {0xd7000,{0x56,0x53,0x48,0x83,0xec,0x28,0x66,0x0f,0xef,0xc0,0x48,0x8d,0x41,0x18,0xc6,0x41}}};
    if(image)return image==module||fail(e,"Subscriber retirement module changed");
    for(const auto& code:codes){std::array<unsigned char,16> actual{};if(!memory.read(module+code.rva,actual.data(),actual.size(),e))return false;if(actual!=code.bytes)return fail(e,"Subscriber retirement original ABI bytes differ RVA="+std::to_string(code.rva));}
    image=module;e.clear();return true;
}
void* entrypoint(){return image?reinterpret_cast<void*>(image+0x2ef4c0):nullptr;}
bool bind(const live_host_state::day::economy::Owners& owner,std::string& e){
    if(!image||owner.history.pin.image!=image)return fail(e,"Subscriber retirement prepared image differs");
    const auto& a=bound.history.pin;const auto& b=owner.history.pin;
    if(a.context!=b.context||a.binding!=b.binding||a.common_epoch!=b.common_epoch){if(active)return fail(e,"Subscriber retirement rebind during original call");retired.clear();blocked.clear();latest=nullptr;}
    bound={};bound.history.pin=owner.history.pin;bound.subscriber_manager=owner.subscriber_manager;first::ClientWorld actual;if(!current(actual,e))return false;
    if(!blocked.empty())return fail(e,blocked);e.clear();return true;
}
void observe(void* manager,void* subscriber,Remove original){
    if(!original)throw std::runtime_error("Subscriber retirement original trampoline missing");
    action_session::Context context;std::string cause;
    if(!bound.history.pin.world||!action_session::selected_session().current(context,cause)||context!=bound.history.pin.context){original(manager,subscriber);return;}
    if(active){blocked="Subscriber retirement original Manager callback reentered; no replay";original(manager,subscriber);return;}
    const bool host=context.role==action_session::Role::Host;
    if(!host&&!client_permit){original(manager,subscriber);return;}
    active=true;struct Leave{~Leave(){active=false;}} leave;
    first::ClientWorld world;local::Census before;int32_t id=-1,reason=-1;uint64_t uid=0;uint8_t dispatch=0;
    const bool ready=prepare(world,uintptr_t(manager),uintptr_t(subscriber),before,id,reason,uid,dispatch,cause);
    latest={{"attempted",true},{"confirmed",false},{"unknown",false},{"id",id},{"reason",reason},{"event_dispatch_enabled",dispatch},{"event_evidence","original Manager path calls RemoveSubscriberEvent/SendEvent"},{"independent_event_ack",false}};
    try{original(manager,subscriber);}catch(...){blocked="Subscriber original Manager retirement threw; no replay";latest["unknown"]=true;latest["cause"]=blocked;throw;}
    if(!ready||!confirm(world,before,uintptr_t(subscriber),id,uid,cause)){blocked="Subscriber original Manager retirement readback Unknown original="+cause;latest["unknown"]=true;latest["cause"]=blocked;return;}
    retired.emplace(id,reason);latest["confirmed"]=true;latest["original_manager_returned"]=true;latest["swap_census_confirmed"]=true;latest["toon_was_present"]=uid!=0;latest["toon_registry_checked"]=uid!=0;latest["toon_registry_absent"]=true;latest["offline_actor_confirmed"]=true;
}
bool facts(std::vector<std::pair<int32_t,int32_t>>& out,std::string& e){out.clear();if(!blocked.empty())return fail(e,blocked);if(bound.history.pin.context.role!=action_session::Role::Host)return fail(e,"Subscriber retirement facts require Host");for(const auto& row:retired)out.push_back(row);e.clear();return true;}
bool apply(int32_t id,int32_t reason,Json& receipt,std::string& e){
    receipt={{"attempted",false},{"confirmed",false},{"unknown",false}};if(active||client_permit||!blocked.empty()||retired.contains(id)||id<0||reason<0)return fail(e,"Subscriber retirement Client duplicate/reentrant/locked/invalid request");
    first::ClientWorld world;local::Census census;if(!current(world,e)||bound.history.pin.context.role!=action_session::Role::Client||!first::read_cohort(memory,world,census,e))return false;
    auto found=std::find_if(census.members.begin(),census.members.end(),[&](const auto& row){return row.id==id;});
    if(found==census.members.end()||found->online)return fail(e,"Subscriber retirement Client actual ID absent or still online");
    receipt["attempted"]=true;client_permit=true;struct Permit{~Permit(){client_permit=false;}} permit;
    try{const auto code=unsubscribe(found->subscriber,reason);if(code){blocked="Subscriber original offline Unsubscribe SEH="+std::to_string(code);receipt["unknown"]=true;return fail(e,blocked);}
        int32_t actual=-1;if(!at(found->subscriber,0x120,actual,e)||actual!=reason){blocked="Subscriber original offline Unsubscribe reason readback Unknown";receipt["unknown"]=true;return fail(e,blocked);}
        const auto removed=remove(world.manager,found->subscriber);if(removed){blocked="Subscriber original Manager retirement SEH="+std::to_string(removed);receipt["unknown"]=true;return fail(e,blocked);}
        receipt=latest;if(!receipt.value("confirmed",false)||receipt.value("id",-1)!=id||receipt.value("reason",-1)!=reason)return fail(e,blocked.empty()?"Subscriber original Manager retirement receipt missing":blocked);
        e.clear();return true;
    }catch(const std::exception& x){blocked="Subscriber Client retirement exception original="+std::string(x.what());receipt["unknown"]=true;return fail(e,blocked);}catch(...){blocked="Subscriber Client retirement nonstandard exception";receipt["unknown"]=true;return fail(e,blocked);}
}
}
