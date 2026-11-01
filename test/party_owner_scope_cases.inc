#include "party_live.h"
#include "native_open_witness.h"
#include "mt2mp/game/native_engine.h"
#include <atomic>
#include <cmath>

namespace party_owner_fixture {
namespace live_host_state=mt2mp::game::live_host_state;
namespace party_live=mt2mp::game::party_live;
namespace party_identity=mt2mp::game::party_identity;
namespace building_mixed_identity=mt2mp::game::building_mixed_identity;
constexpr const char* executable="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6";
std::vector<std::string> calls;
bool session_ok=true,world_ok=true,resolver_ok=true;
bool fail(std::string& error,const std::string& cause){error=cause;return false;}
namespace action_session {
using Context=mt2mp::action_session::Context;
using Binding=mt2mp::action_session::Binding;
using Role=mt2mp::action_session::Role;
struct Session {Context value;bool current(Context& out,std::string& error){
    calls.push_back("session");if(!session_ok)return fail(error,"fixture session unavailable");out=value;return true;
}} session;
Session& selected_session(){return session;}
}
namespace native_open_runtime {
building_mixed_identity::opened_file::CurrentWorld value;
bool read_current_world(decltype(value)& out,std::string& error){
    calls.push_back("world");if(!world_ok)return fail(error,"fixture world unavailable");out=value;return true;
}
}
namespace native {
using ClockState=mt2mp::game::native::ClockState;
using WorldStatus=mt2mp::game::native::WorldStatus;
std::atomic<uintptr_t> image=0;
std::atomic<uint32_t> game_thread=0;
std::atomic<bool> world_available=false,title_available=false;
constexpr uintptr_t clock_slot=0x13df9a0,mode_slot=0x13df320,game_slot=0x13df8f0;
bool read_resolver_receipt(uintptr_t& base,uint32_t& thread,std::string& sha,std::string& error){
    calls.push_back("resolver");if(!resolver_ok)return fail(error,"fixture resolver unavailable");
    base=image.load();thread=game_thread.load();sha=executable;return true;
}
#include "party_owner_engine.inc"
}
struct Memory {
    std::vector<std::pair<uintptr_t,size_t>> reads;
    bool read(uintptr_t pointer,void* out,size_t count,std::string& error){
        reads.emplace_back(pointer,count);SIZE_T actual=0;
        if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(pointer),out,count,&actual)||actual!=count)
            return fail(error,"fixture Party read failed");return true;
    }
} memory;
template<class T>bool field(uintptr_t object,size_t offset,T& out,std::string& error){
    if(!object||offset>UINTPTR_MAX-object)return fail(error,"Party live field extent overflows");
    return memory.read(object+offset,&out,sizeof out,error);
}
#include "party_owner_scheduler.inc"
struct NativePort {
    action_session::Context context_;
    action_session::Binding binding_;
    party_live::Raw current_;
    uintptr_t image_=0;uint32_t thread_=0;bool held_=false;
    party_identity::Scope blocked_scope_;
    std::string blocked_cause_;
    bool actual(std::string&);bool prior(std::string&);
};
#include "party_owner_actual.inc"
struct Fixture {
    void* allocation=VirtualAlloc(nullptr,0x1400000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    uintptr_t base=uintptr_t(allocation),game=base+0x1000,mode=base+0x2000,clock=base+0x3000;
    uintptr_t tasks=base+0x4000,ticks=base+0x5000,updates=base+0x6000;
    NativePort port;
    template<class T>void put(uintptr_t address,T value){std::memcpy(reinterpret_cast<void*>(address),&value,sizeof value);}
    Fixture(){
        need(allocation!=nullptr,"Party owner fixture allocation failed");
        native::image=base;native::game_thread=GetCurrentThreadId();session_ok=world_ok=resolver_ok=true;
        put(base+native::game_slot,game);put(base+native::mode_slot,mode);put(base+native::clock_slot,clock);
        put(game,base+0x1743740);put(game+0x28,mode);put(game+0x358,mode);put(game+0x434,int32_t(3));
        put(mode,base+0x16e6730);put(mode+0x58,base+0x7000);put(mode+0x2c4,int32_t(4));
        put(clock+0x58,0.0f);put(clock+0x5c,1.0f);put(clock+0x44,int32_t(23));
        put(base+0x13df340,tasks);put(base+0x13df350,ticks);put(base+0x13df4a0,updates);
        put(tasks,base+0x16fdaf0);put(ticks,base+0x16fdc40);put(updates,base+0x1709650);
        put(game+0x68,tasks);put(game+0x150,ticks);put(game+0x70,updates);
        auto& world=native_open_runtime::value;world={};world.game=reinterpret_cast<void*>(game);world.mode=reinterpret_cast<void*>(mode);
        world.state=reinterpret_cast<void*>(base+0x8000);world.world=reinterpret_cast<void*>(base+0x7000);
        world.map=reinterpret_cast<void*>(base+0x9000);world.uuid="fixture-party-world";world.world_epoch=31;world.generation=47;
        port.context_.role=action_session::Role::Client;port.context_.snapshot_uuid=world.uuid;
        action_session::session.value=port.context_;port.binding_={31,47};port.image_=base;port.thread_=GetCurrentThreadId();
        auto& raw=port.current_;raw.scope.snapshot=world.uuid;raw.scope.epoch=31;raw.scope.generation=47;raw.scope.root=base+0x91c0;
        raw.world.game=world.game;raw.world.mode=world.mode;raw.world.state=world.state;raw.world.world=world.world;raw.world.map=world.map;
    }
    ~Fixture(){if(allocation&&!VirtualFree(allocation,0,MEM_RELEASE))std::terminate();}
    void held(bool value){port.held_=value;put(mode+0x30c,int32_t(value));put(tasks+0xf8,uint8_t(value));put(ticks+0x28,uint8_t(value));put(updates+0x68,uint8_t(value));}
    std::pair<uint64_t,uint64_t> compare(bool success){
        std::string old_error,new_error;auto* enclosing=t::page_read_scope;const bool enclosing_reader=t::PageReadScope::reader_active();
        memory.reads.clear();calls.clear();auto start=count(t::Part::QueryEngine);
        native::world_available=false;native::title_available=false;
        const bool old=port.prior(old_error);const auto old_queries=count(t::Part::QueryEngine)-start;
        const auto old_reads=memory.reads;const auto old_calls=calls;const bool old_world=native::world_available.load(),old_title=native::title_available.load();
        memory.reads.clear();calls.clear();start=count(t::Part::QueryEngine);
        native::world_available=false;native::title_available=false;
        const bool actual=port.actual(new_error);const auto actual_queries=count(t::Part::QueryEngine)-start;
        need(old==success&&actual==old&&new_error==old_error,"Party owner success or original rejection cause changed");
        need(old_reads==memory.reads&&old_calls==calls,"Party owner scheduler reads or outer proof calls changed");
        need(old_world==native::world_available.load()&&old_title==native::title_available.load(),"Party owner Engine availability side effects changed");
        need(t::page_read_scope==enclosing&&t::PageReadScope::reader_active()==enclosing_reader,"Party owner reader escaped its pure-call boundary");
        return {old_queries,actual_queries};
    }
};
void run(){
    Fixture f;t::Scope take(t::Part::Take);
    const auto queries=f.compare(true);std::cout<<"Party owner Engine queries="<<queries.first<<"->"<<queries.second<<'\n';
    need(queries.first==55&&queries.second==2,"Party owner did not retain the complete reads with one suffix and one prefix metadata query");
    f.held(true);f.compare(true);f.held(false);
    f.port.context_.role=action_session::Role::Host;action_session::session.value=f.port.context_;f.compare(true);
    f.put(f.clock+0x58,1.0f);f.compare(true);
    f.port.context_.role=action_session::Role::Client;action_session::session.value=f.port.context_;f.compare(false);f.put(f.clock+0x58,0.0f);
    session_ok=false;f.compare(false);session_ok=true;world_ok=false;f.compare(false);world_ok=true;resolver_ok=false;f.compare(false);resolver_ok=true;
    ++native_open_runtime::value.generation;f.compare(false);--native_open_runtime::value.generation;
    f.port.blocked_scope_=f.port.current_.scope;f.port.blocked_cause_="retained blocked cause";f.compare(false);f.port.blocked_cause_.clear();
    f.put(f.game+0x434,int32_t(30));f.compare(false);f.put(f.game+0x434,int32_t(3));
    f.put(f.game+0x428,f.mode);f.compare(false);f.put(f.game+0x428,uintptr_t(0));
    f.put(f.mode+0x2c4,int32_t(0));f.compare(false);f.put(f.mode+0x2c4,int32_t(4));
    f.put(f.mode+0x304,uint8_t(1));f.compare(false);f.put(f.mode+0x304,uint8_t(0));
    f.put(f.base+0x13beda0,int32_t(1));f.compare(false);f.put(f.base+0x13beda0,int32_t(0));
    f.put(f.mode+0x30c,int32_t(1));f.compare(false);f.put(f.mode+0x30c,int32_t(0));
    f.put(f.clock+0x5c,std::numeric_limits<float>::quiet_NaN());f.compare(false);f.put(f.clock+0x5c,1.0f);
    f.put(f.game+0x68,f.ticks);f.compare(false);f.put(f.game+0x68,f.tasks);
    f.put(f.ticks+0x28,uint8_t(1));f.compare(false);f.put(f.ticks+0x28,uint8_t(0));
    f.held(true);f.put(f.tasks+0xa8,int32_t(1));f.compare(false);f.put(f.tasks+0xa8,int32_t(0));f.held(false);
    DWORD prior=0,ignored=0;need(VirtualProtect(reinterpret_cast<void*>(f.mode),4096,PAGE_NOACCESS,&prior)!=0,"Party owner page protection failed");
    f.compare(false);need(VirtualProtect(reinterpret_cast<void*>(f.mode),4096,prior,&ignored)!=0,"Party owner page restore failed");f.compare(true);
    {t::PageReadScope parent;f.compare(true);{t::PageReadScope callback(t::PageBoundary::OriginalCall);f.compare(true);}f.compare(true);}
    std::cout<<"Party owner: same complete native status/clock/scheduler checks; Engine OS queries "<<queries.first<<" -> "<<queries.second<<'\n';
}
}
