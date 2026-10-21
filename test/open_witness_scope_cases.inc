#include "native_open_witness.h"
#include <atomic>
#include <mutex>
#include <tuple>

namespace open_witness_fixture {
using mt2mp::game::building_mixed_identity::opened_file::Status;
using mt2mp::game::building_mixed_identity::opened_file::CurrentWorld;
using mt2mp::game::building_mixed_identity::opened_file::required_hooks;
#include "open_witness_type.inc"
std::mutex gate;
Status current;
uintptr_t image=0;
void* native_mode=nullptr;
WorldWitness loaded_world;
uint64_t world_epoch=0;
enum class InitPhase {Uninitialized,Ready,Failed};
InitPhase init_phase=InitPhase::Uninitialized;
uint32_t installed_hooks=0;
std::atomic<bool> callback_failure=false;
struct Read {const void* address=nullptr;size_t bytes=0;std::vector<unsigned char> value;bool operator==(const Read&)const=default;};
std::vector<Read> reads;
size_t fail_copy=0;
int scenario=0;
bool expect_scoped=false;
t::PageReadScope* expected_parent=nullptr;
unsigned char* objects=nullptr;
DWORD copy(const void*,void*,size_t);
#include "open_witness_bodies.inc"
template<class T> void put(void* object,size_t offset,T value){
    std::memcpy(static_cast<unsigned char*>(object)+offset,&value,sizeof value);
}
DWORD copy(const void* from,void* to,size_t bytes){
    need((t::page_read_scope!=nullptr)==expect_scoped,"witness copy scope lifetime differs");
    if(expected_parent)need(t::page_read_scope==expected_parent,"existing parent scope was replaced");
    reads.push_back({from,bytes,{}});
    if(reads.size()==15&&scenario==23)throw std::runtime_error("witness copy exception");
    if(reads.size()==15&&scenario==19){
        put(reinterpret_cast<void*>(image),0x13df8c0,static_cast<void*>(objects+0xe00));
        put(objects+0x900,0x158,static_cast<void*>(objects+0xe00));
    }
    if(reads.size()==15&&scenario==22){
        DWORD previous=0;
        need(VirtualProtect(reinterpret_cast<void*>(image+0x13df000),4096,PAGE_NOACCESS,&previous)!=0,
             "create second-pass real copy fault");
    }
    const auto code=reads.size()==fail_copy?DWORD(EXCEPTION_ACCESS_VIOLATION):native_copy(from,to,bytes);
    if(!code)reads.back().value.assign(static_cast<unsigned char*>(to),static_cast<unsigned char*>(to)+bytes);
    if(reads.size()==28&&scenario==20)callback_failure.store(true);
    return code;
}
auto fields(const CurrentWorld& w){return std::tuple(w.game,w.mode,w.state,w.world,w.map,
    w.generation,w.world_epoch,w.bytes,w.uuid,w.sha256);}
struct Outcome {
    bool ok=false;CurrentWorld result;std::string error;Status status;std::vector<Read> sequence;
    uint64_t queries=0,resets=0;
};
void reset(){
    DWORD previous=0;
    need(VirtualProtect(reinterpret_cast<void*>(image+0x13df000),4096,PAGE_READWRITE,&previous)!=0,
         "restore singleton page");
    need(VirtualProtect(objects,4096,PAGE_READWRITE,&previous)!=0,"restore witness object page");
    current={};current.state="ready";current.generation=37;current.game_thread=GetCurrentThreadId();
    current.completed_origin=true;current.current_origin_available=true;current.world_witness_bound=true;
    current.origin_confirmed=true;current.reader_confirmed=true;current.origin_world_epoch=41;
    current.uuid="fixture-world";current.sha256="fixture-hash";current.bytes=1234;
    world_epoch=41;init_phase=InitPhase::Ready;installed_hooks=required_hooks;callback_failure.store(false);
    native_mode=objects;loaded_world={objects+0x300,objects,objects+0x600,objects+0x900,objects+0xc00};
    put(reinterpret_cast<void*>(image),0x13df320,native_mode);
    put(reinterpret_cast<void*>(image),0x13df8f0,loaded_world.game);
    put(reinterpret_cast<void*>(image),0x13df9c0,loaded_world.world);
    put(reinterpret_cast<void*>(image),0x13df8c0,loaded_world.map);
    put(objects,0,image+0x16e6730);put(objects,0x2c4,int32_t(4));put(objects,0x58,loaded_world.state);
    put(objects+0x300,0,image+0x1743740);put(objects+0x300,0x28,native_mode);
    put(objects+0x600,0,image+0x16de920);put(objects+0x600,0x160,loaded_world.world);
    put(objects+0x900,0,image+0x174aef0);put(objects+0x900,0x158,loaded_world.map);
    put(objects+0xc00,0,image+0x1742860);put(objects+0xe00,0,image+0x1742860);
    reads.clear();
}
Outcome run(bool candidate,int kind,size_t failure=0,bool parent=false){
    reset();scenario=kind;fail_copy=failure;
    uint64_t generation=37;
    switch(kind){
    case 1:++current.game_thread;break;
    case 2:generation=0;break;
    case 3:++generation;break;
    case 4:init_phase=InitPhase::Failed;break;
    case 5:installed_hooks=0;break;
    case 6:callback_failure.store(true);break;
    case 7:current.completed_origin=false;break;
    case 8:current.current_origin_available=false;break;
    case 9:current.world_witness_bound=false;break;
    case 10:current.origin_world_epoch=0;break;
    case 11:++world_epoch;break;
    case 12:current.state="failed";break;
    case 13:current.state="invalidated";break;
    case 14:loaded_world.map=objects+0xe00;break;
    case 15:native_mode=objects+0x300;break;
    case 16:put(objects,0,uintptr_t(0));break;
    case 17:put(objects,0x2c4,int32_t(3));break;
    case 18:put(objects+0x900,0x158,static_cast<void*>(objects+0xe00));break;
    case 21:{DWORD previous=0;need(VirtualProtect(objects,4096,PAGE_NOACCESS,&previous)!=0,
                                 "create unreadable witness objects");break;}
    default:break;
    }
    Outcome out;out.result.uuid="must be cleared";
    t::Scope take(t::Part::Take);
    {
        t::PageReadScope caller(parent);
        expected_parent=parent?t::page_read_scope:nullptr;
        expect_scoped=parent||candidate;
        MEMORY_BASIC_INFORMATION info{};
        int sentinel=7;
        if(parent)need(t::virtual_query(&sentinel,&info,sizeof info)==sizeof info,"prime caller metadata");
        const auto before=count(t::Part::MemoryVirtualQuery),resets=count(t::Part::ReaderNestedReset);
        out.ok=candidate?open_witness_fixture::read_completed_world(generation,out.result,out.error):
                         baseline_read_completed_world(generation,out.result,out.error);
        out.queries=count(t::Part::MemoryVirtualQuery)-before;
        out.resets=count(t::Part::ReaderNestedReset)-resets;
        need(t::page_read_scope==expected_parent,"witness scope extends beyond the two calls");
        if(parent){
            const auto after=count(t::Part::MemoryVirtualQuery);
            need(t::virtual_query(&sentinel,&info,sizeof info)==sizeof info&&count(t::Part::MemoryVirtualQuery)==after,
                 "witness cleared existing caller metadata");
        }
    }
    expected_parent=nullptr;
    out.status=current;out.sequence=reads;
    need(t::page_read_scope==nullptr,"witness fixture leaves scope active");
    return out;
}
void same(const Outcome& old,const Outcome& now,uint64_t expected_resets=0){
    need(old.ok==now.ok&&old.error==now.error&&fields(old.result)==fields(now.result),
         "actual completed-world result or failure cause changed");
    need(old.sequence==now.sequence,"actual completed-world copy sequence or bytes changed");
    need(old.status.world_checks==now.status.world_checks&&old.status.state==now.status.state&&
         old.status.error==now.status.error&&old.status.origin_confirmed==now.status.origin_confirmed&&
         old.status.reader_confirmed==now.status.reader_confirmed&&
         old.status.current_origin_available==now.status.current_origin_available,
         "actual completed-world failure state or attempted-pass count changed");
    need(now.resets==expected_resets,"conditional witness scope reset count differs");
}
}
void open_witness_scope_cases(){
    using namespace open_witness_fixture;
    struct Owned {
        void* image_region=nullptr;void* object_region=nullptr;
        ~Owned(){for(auto p:{image_region,object_region})if(p&&!VirtualFree(p,0,MEM_RELEASE))
            std::cerr<<"witness fixture release failed "<<GetLastError()<<'\n';}
    } owned;
    owned.image_region=VirtualAlloc(nullptr,0x13e0000,MEM_RESERVE,PAGE_NOACCESS);
    need(owned.image_region!=nullptr,"reserve owned singleton address range");
    image=reinterpret_cast<uintptr_t>(owned.image_region);
    need(VirtualAlloc(reinterpret_cast<void*>(image+0x13df000),4096,MEM_COMMIT,PAGE_READWRITE)!=nullptr,
         "commit only actual singleton page");
    owned.object_region=VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    need(owned.object_region!=nullptr,"allocate owned witness objects");
    objects=static_cast<unsigned char*>(owned.object_region);
    prefix_fixture::observation.enabled=false;t::totals={};
    const auto old=run(false,0),now=run(true,0);same(old,now);
    need(now.ok&&now.sequence.size()==28&&now.status.world_checks==2,"both complete production witness passes execute");
    need(old.queries==28&&now.queries==2,"two-page fixture retains 28 copies with only two metadata queries");
    for(int kind=1;kind<=22;++kind){
        const auto before=run(false,kind),after=run(true,kind);same(before,after);
        need(!after.ok&&fields(after.result)==fields(CurrentWorld{}),"failed witness returns no partially confirmed world");
        if(kind<=13)need(after.sequence.empty()&&after.queries==0,"admission failure performs no field read or query");
        if(kind==19)need(after.sequence.size()==28&&after.error=="current_world_read_witness_changed",
                          "second full pass observes changed reciprocal map values");
        if(kind==20)need(after.sequence.size()==28&&after.error=="current_world_read_observer_failure",
                          "post-read callback failure remains visible");
        if(kind==22)need(after.sequence.size()==15,"second-pass actual SEH copy failure remains visible");
    }
    for(size_t failure=1;failure<=28;++failure){
        const auto before=run(false,0,failure),after=run(true,0,failure);same(before,after);
        need(!after.ok&&after.sequence.size()==failure,"injected copy fault stops at same attempted field");
    }
    for(int kind:{0,19,20}){
        const auto before=run(false,kind,0,true),after=run(true,kind,0,true);same(before,after);
        need(before.queries==after.queries&&before.resets==after.resets,
             "existing parent retains identical metadata work and reset count");
    }
    reset();scenario=0;fail_copy=0;expect_scoped=true;
    t::totals={};CurrentWorld result;std::string error;
    need(open_witness_fixture::read_completed_world(37,result,error)&&reads.size()==28&&count(t::Part::MemoryVirtualQuery)==0&&
         t::page_read_scope==nullptr,"outside Take keeps full reader and no diagnostic events");

    // A native callback boundary is not a reader whose metadata can be borrowed.
    const auto callback_read=[&](bool candidate,int kind,size_t failure=0){
        reset();scenario=kind;fail_copy=failure;expect_scoped=true;expected_parent=nullptr;
        Outcome out;
        if(kind==21){DWORD previous=0;need(VirtualProtect(objects,4096,PAGE_NOACCESS,&previous)!=0,
            "callback fixture denies object page");}
        {
            t::Scope take(t::Part::Take);
            t::PageReadScope parent;
            MEMORY_BASIC_INFORMATION info{};int sentinel=7;
            need(t::virtual_query(&sentinel,&info,sizeof info)==sizeof info,"prime outer callback metadata");
            {
                t::PageReadScope original(t::PageBoundary::OriginalCall);
                auto* boundary=t::page_read_scope;
                const auto queries=count(t::Part::MemoryVirtualQuery),resets=count(t::Part::ReaderNestedReset);
                out.ok=candidate?open_witness_fixture::read_completed_world(37,out.result,out.error):
                    historical_read_completed_world(37,out.result,out.error);
                out.queries=count(t::Part::MemoryVirtualQuery)-queries;
                out.resets=count(t::Part::ReaderNestedReset)-resets;
                need(t::page_read_scope==boundary&&!t::PageReadScope::reader_active(),
                     "witness restores the enclosing original boundary");
            }
            const auto queries=count(t::Part::MemoryVirtualQuery);
            need(t::virtual_query(&sentinel,&info,sizeof info)==sizeof info&&
                 count(t::Part::MemoryVirtualQuery)==queries+1,"callback return invalidates outer metadata");
        }
        out.status=current;out.sequence=reads;
        need(t::page_read_scope==nullptr,"callback reader leaves no cached metadata");
        return out;
    };
    for(int kind:{0,19,20,21,22}){
        const auto old_callback=callback_read(false,kind),new_callback=callback_read(true,kind);
        same(old_callback,new_callback,2);
        if(kind==0)need(old_callback.queries==28&&new_callback.queries==2,
            "actual callback witness keeps 28 copies and reduces metadata queries from 28 to 2");
        else need(!new_callback.ok,"callback reader preserves witness/callback/protection failures");
    }
    for(size_t failure=1;failure<=28;++failure)
        same(callback_read(false,0,failure),callback_read(true,0,failure),2);
    reset();scenario=0;fail_copy=0;expect_scoped=true;expected_parent=nullptr;
    {
        t::Scope host(t::Part::HostCapture);
        t::PageReadScope original(t::PageBoundary::OriginalCall);
        auto* boundary=t::page_read_scope;
        const auto before=count(t::Part::HostMemoryVirtualQuery);
        SetLastError(123);
        need(open_witness_fixture::read_completed_world(37,result,error)&&GetLastError()==123,
             "Host callback reader preserves successful LastError");
        const auto first=count(t::Part::HostMemoryVirtualQuery);
        need(first==before+2&&reads.size()==28,"Host callback query count uses actual Host timing");
        need(open_witness_fixture::read_completed_world(37,result,error)&&
             count(t::Part::HostMemoryVirtualQuery)==first+2&&reads.size()==56,
             "successive callback readers query afresh and repeat every live field");
        put(objects+0x900,0x158,static_cast<void*>(objects+0xe00));
        need(!open_witness_fixture::read_completed_world(37,result,error)&&
             error=="completed_origin_GameState_World_Map_reciprocity_mismatch"&&
             fields(result)==fields(CurrentWorld{}),"next callback reader observes changed world owner");
        reset();scenario=23;bool caught=false;
        try{open_witness_fixture::read_completed_world(37,result,error);}
        catch(const std::runtime_error& problem){caught=std::string(problem.what())=="witness copy exception";}
        need(caught&&t::page_read_scope==boundary&&!t::PageReadScope::reader_active(),
             "reader exception propagates and restores the original callback boundary");
    }
    need(t::page_read_scope==nullptr,"Host callback scope cannot leak to the next capture");
    t::totals={};
}
