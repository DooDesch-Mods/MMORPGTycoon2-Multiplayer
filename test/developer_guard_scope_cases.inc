#include "bug_authority/fingerprints.h"
#include "bug_authority/job_state.h"
#include "npc_quest_daily_apply.h"
namespace actual_guards {
uint64_t reads=0;uintptr_t failed=0;bool throws=false;
struct Memory {bool copy(uintptr_t p,void* out,size_t n,std::string& e){
    ++reads;if(throws)throw std::runtime_error("original guard read exception");
    if(p==failed){e="original guard read failure";return false;}return memory.copy(p,out,n,e);
}};
struct Runtime {uintptr_t image=0,tracker=0;bool ready=true;Memory memory;};
Runtime owned;Runtime& runtime(){return owned;}
struct Hook {uintptr_t rva=0,trampoline=0;size_t size=0;std::vector<unsigned char> installed;std::array<unsigned char,64> relocated{};};
struct Body {uintptr_t rva=0;size_t size=0;std::string sha;};
std::vector<Hook> hooks;std::vector<Body> original_bodies;
bool fail(std::string& e,const std::string& cause){e=cause;return false;}
#include "scoped_bug_code.inc"
using Capture=staff::Capture;using JobCounter=mt2mp::game::bug_authority::JobCounter;
namespace npc_quest_daily_apply=mt2mp::npc_quest_daily_apply;
template<class T>bool field(uintptr_t p,size_t o,T& value,std::string& e){return owned.memory.copy(p+o,&value,sizeof value,e);}
#include "scoped_bug_tracker.inc"
#include "scoped_job_counter.inc"
namespace action {enum class Role{Client,Host};struct Context{Role role=Role::Client;};}
using PoolState=int;
staff::Capture expected_capture;bool read_ok=true;PoolState fixture_observed=7;
namespace action_runtime {bool read(Runtime&,const action::Context&,int,staff::Capture& out,PoolState& pool,std::string& e){
    if(!read_ok){e="original Developer pool failure";return false;}out=expected_capture;pool=fixture_observed;return true;
}}
struct FrameCalls {Runtime& runtime;action::Context context;int binding=0;staff::Capture before;
#include "scoped_developer_pin.inc"
};
}
void guard_scope_cases(const staff::Capture& capture){
    namespace g=actual_guards;g::owned.image=image;g::hooks.clear();g::original_bodies.clear();
    const std::set<uintptr_t> hooked={0x58da0,0xc2e810,0x59fb0,0x59a00,0x58740,0x5d100,0xdc1800,0xdc17d0,0x8acc80,0xbaded0};
    for(const auto& spec:mt2mp::game::bug_authority::original_bodies){
        std::vector<unsigned char> bytes(spec.size);for(size_t i=0;i<bytes.size();++i)bytes[i]=static_cast<unsigned char>((i+spec.rva)%251);
        std::memcpy(reinterpret_cast<void*>(image+spec.rva),bytes.data(),bytes.size());
        g::original_bodies.push_back({spec.rva,spec.size,mt2mp::sha256(bytes)});
        if(hooked.contains(spec.rva)){g::Hook h;h.rva=spec.rva;h.size=spec.size;h.installed=bytes;h.trampoline=image+0x150000+g::hooks.size()*128;
            h.relocated.fill(static_cast<unsigned char>(g::hooks.size()+1));std::memcpy(reinterpret_cast<void*>(h.trampoline),h.relocated.data(),h.relocated.size());g::hooks.push_back(std::move(h));}
    }
    need(g::hooks.size()==10&&g::original_bodies.size()==33,"actual shipped ten-hook/33-body read count shape");
    std::string e;auto q=queries();g::reads=0;
    for(int i=0;i<8;++i)need(g::native_code(e),e);
    need(g::reads==344&&queries()-q<40,"eight actual native_code bodies preserve344 live copies with independent reader scopes");
    std::cout<<"native_code eight calls live reads="<<g::reads<<", OS queries="<<queries()-q<<'\n';
    need(timing::page_read_scope==nullptr,"native_code closes before any original hook invocation");
    const auto& h=g::hooks.front();put(image+h.rva,0,uint8_t(255));need(!g::native_code(e)&&e.find("Bug installed native hook owner/body/trampoline changed RVA=")==0,"changed installed hook still rejected");
    std::memcpy(reinterpret_cast<void*>(image+h.rva),h.installed.data(),h.installed.size());put(h.trampoline,0,uint8_t(255));
    need(!g::native_code(e)&&e.find("Bug installed native hook owner/body/trampoline changed RVA=")==0,"changed trampoline still rejected");std::memcpy(reinterpret_cast<void*>(h.trampoline),h.relocated.data(),h.relocated.size());
    const auto& body=g::original_bodies.front();put(image+body.rva,0,uint8_t(255));need(!g::native_code(e)&&e.find("Bug original complete native body changed RVA=")==0,"fresh SHA256 rejects changed unhooked code");
    put(image+body.rva,0,static_cast<uint8_t>(body.rva%251));
    g::failed=image+h.rva;need(!g::native_code(e)&&e=="original guard read failure","original code read error retained");g::failed=0;
    g::throws=true;bool caught=false;try{g::native_code(e);}catch(const std::runtime_error& x){caught=std::string(x.what())=="original guard read exception";}
    g::throws=false;need(caught&&timing::page_read_scope==nullptr,"code exception closes reader without suppression");
    g::expected_capture=capture;g::FrameCalls frame{g::owned,{},0,{}};g::reads=0;q=queries();
    need(frame.pin(7,e)&&frame.pin(7,e),e);need(g::reads==100&&queries()-q<10,"two actual FrameCalls pin loops retain all100 carried-owner reads");
    need(timing::page_read_scope==nullptr,"carried guard scope ends before scalar effects");
    put(dev(7),0x4d0,uintptr_t(9));need(!frame.pin(7,e)&&e=="Developer frame actual carried Monster closure is not yet bound","fresh carried Monster rejects");put(dev(7),0x4d0,uintptr_t(0));
    frame.context.role=g::action::Role::Host;g::reads=0;need(!frame.pin(7,e)&&e=="Developer frame requires exact actual Client BEFORE pool"&&!g::reads,"Host rejected before carried reader");frame.context.role=g::action::Role::Client;
    g::fixture_observed=8;need(!frame.pin(7,e)&&e=="Developer frame requires exact actual Client BEFORE pool","different original pool rejected");g::fixture_observed=7;
    g::read_ok=false;need(!frame.pin(7,e)&&e=="original Developer pool failure","original pool failure retained");g::read_ok=true;
    g::failed=dev(4)+0x4d0;need(!frame.pin(7,e)&&e=="original guard read failure","carried field failure retained");g::failed=0;
    DWORD old=0;need(VirtualProtect(reinterpret_cast<void*>(dev(0)),4096,PAGE_NOACCESS,&old)!=0,"change carried page after reader");
    need(!frame.pin(7,e)&&e=="error: native placement UI page unreadable","next carried guard sees current protection");DWORD ignored=0;
    need(VirtualProtect(reinterpret_cast<void*>(dev(0)),4096,old,&ignored)!=0,"restore owned actor page");need(frame.pin(7,e),e);
    g::owned.tracker=image+0x1b000;const auto members=image+0x1c000;put(g::owned.tracker,0x18,members);put(g::owned.tracker,0x20,int32_t(0));put(g::owned.tracker,0x24,int32_t(2));
    std::vector<uintptr_t> tracked;g::reads=0;q=queries();
    for(int i=0;i<4;++i)need(g::tracker_members(tracked,e)&&tracked.empty(),e);
    need(g::reads==12&&queries()-q==4,"four empty Tracker reads retain12 live guards and independent scopes");
    put(g::owned.tracker,0x20,int32_t(2));put(members,0,uintptr_t(17));put(members,8,uintptr_t(18));
    need(g::tracker_members(tracked,e)&&tracked==std::vector<uintptr_t>{17,18},"nonempty Tracker order and receivers read live");
    put(members,8,uintptr_t(17));need(!g::tracker_members(tracked,e)&&e=="Bug Tracker live member duplicate/null","Tracker duplicate remains rejected");put(members,8,uintptr_t(0));
    need(!g::tracker_members(tracked,e)&&e=="Bug Tracker live member duplicate/null","Tracker null remains rejected");
    const auto queue=image+0x1d000,daily=queue+0x48,history=image+0x1e000;
    put(daily,0,image+0x16e4710);put(daily,0x38,image+0x1749370);put(daily,0x40,history);
    put(daily,0x48,std::array<int32_t,8>{1,4,5,6,7,10,8,9});put(daily,0x68,uint8_t(0));put(history,0,int32_t(3));
    g::JobCounter counter;g::reads=0;q=queries();
    for(int i=0;i<8;++i)need(g::read_counter(capture,queue,0x48,counter,e),e);
    need(g::reads==48&&queries()-q==8,"eight DailyCount reads retain48 live copies with independent scopes");
    need(counter.value.today==5&&counter.value.total==6&&counter.value.history==std::vector<int32_t>{3}&&counter.highest==8&&counter.day==7,"actual DailyCount projection preserves all values");
    put(history,0,int32_t(4));need(g::read_counter(capture,queue,0x48,counter,e)&&counter.value.history[0]==4,"later DailyCount history remains live");
    put(daily,0x68,uint8_t(2));need(!g::read_counter(capture,queue,0x48,counter,e)&&e=="DailyCount history span, retention or warning differs","DailyCount shape failure retained");put(daily,0x68,uint8_t(0));
    g::failed=history;need(!g::read_counter(capture,queue,0x48,counter,e)&&e=="original guard read failure","DailyCount read cause retained");g::failed=0;
    need(timing::page_read_scope==nullptr,"all pure guard scopes close before original effects");
}
