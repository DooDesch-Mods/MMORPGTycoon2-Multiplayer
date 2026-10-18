#include <algorithm>
#include <cstring>
#include <set>
#include <vector>
namespace world_prop_attribution_fixture {
struct Memory {
    bool uid_read=false;
    bool copy(uintptr_t,void* out,size_t bytes,std::string& e){
        if(uid_read&&bytes==sizeof(uint64_t)){const uint64_t uid=77;std::memcpy(out,&uid,bytes);return true;}
        e="retained WorldProp read failure";return false;
    }
};
struct CurrentWorld {
    void* game=nullptr;void* mode=nullptr;void* state=nullptr;void* world=nullptr;void* map=nullptr;
    uint64_t world_epoch=0,generation=0,bytes=0;std::string uuid,sha256;
};
struct WorldState {CurrentWorld origin;uintptr_t root=0,selection=0,active_graph=0;int32_t saves=-1,edits=-1;};
struct Origin {
    bool throws=false;
    bool current(CurrentWorld&,std::string& e){
        if(throws)throw std::runtime_error("retained WorldProp origin exception");
        e="retained WorldProp origin failure";return false;
    }
};
using FindUid=void*(*)(uint64_t);
using DynamicCast=void*(*)(void*,const void*,const void*,ptrdiff_t);
struct LookupApi {FindUid find=nullptr;DynamicCast cast=nullptr;};
inline bool lookup_throws=false,lookup_missing=false;
void* find(uint64_t){if(lookup_throws)throw std::runtime_error("retained WorldProp lookup exception");return lookup_missing?nullptr:reinterpret_cast<void*>(uintptr_t(0x4321));}
void* cast(void* value,const void*,const void*,ptrdiff_t){return value;}
struct Reader {
    Memory& memory_;Origin& origin_;uintptr_t image_=1;uint32_t thread_=GetCurrentThreadId();LookupApi api_{find,cast};bool world_busy_=false;
    bool current(WorldState&,bool,std::string&);
    bool typed(uintptr_t,uint64_t,uintptr_t,std::string&);
    bool main_member(uintptr_t,uintptr_t,uintptr_t,std::string&);
    bool root(const WorldState&,uintptr_t,std::string&);
};
#include "world_prop_attribution_bodies.inc"
bool Reader::root(const WorldState& world,uintptr_t region,std::string& e){
#include "world_prop_attribution_root.inc"
        return root_region(region);
}
}

#include "world_prop_array_cases.inc"

void world_prop_attribution_cases(){
    world_prop_array_cases();
    namespace f=world_prop_attribution_fixture;
    f::Memory memory;f::Origin origin;f::Reader reader{memory,origin};f::WorldState world;std::string error;
    const auto current_before=count(t::Part::WorldPropCurrent);
    reader.thread_=0;
    need(!reader.current(world,false,error)&&error=="remove_world_wrong_thread","actual current early failure unchanged");
    need(count(t::Part::WorldPropCurrent)==current_before+1&&!reader.world_busy_,"actual current failure counted and Busy scope closed");
    reader.thread_=GetCurrentThreadId();error.clear();
    need(!reader.current(world,false,error)&&error=="retained WorldProp origin failure","actual current includes original origin failure");
    need(count(t::Part::WorldPropCurrent)==current_before+2,"actual origin failure completes current timer");
    {
        t::Scope outer(t::Part::DeveloperAdopt);origin.throws=true;bool caught=false;
        try{reader.current(world,false,error);}catch(const std::runtime_error& cause){caught=std::string(cause.what())=="retained WorldProp origin exception";}
        origin.throws=false;
        need(caught&&!reader.world_busy_&&t::query_phase==t::QueryPhase::Developer,"actual origin unwind preserves cause and parent attribution");
        need(count(t::Part::WorldPropCurrent)==current_before+3,"actual current exception recorded once");
        memory.uid_read=true;error.clear();const auto typed_before=count(t::Part::WorldPropTypedLookup);
        need(reader.typed(0x4321,77,0x1234,error),"actual lookup and UID validation result unchanged");
        need(count(t::Part::WorldPropTypedLookup)==typed_before+1&&t::query_phase==t::QueryPhase::Developer,"native lookup timing restores enclosing phase");
        f::lookup_missing=true;error.clear();
        need(!reader.typed(0x4321,77,0x1234,error)&&error=="remove_prop_registry_or_typed_identity","native identity failure retained");f::lookup_missing=false;
        need(count(t::Part::WorldPropTypedLookup)==typed_before+2,"failed native identity lookup counted once");
        f::lookup_throws=true;caught=false;
        try{reader.typed(0x4321,77,0x1234,error);}catch(const std::runtime_error& cause){caught=std::string(cause.what())=="retained WorldProp lookup exception";}
        f::lookup_throws=false;
        need(caught&&count(t::Part::WorldPropTypedLookup)==typed_before+3&&t::query_phase==t::QueryPhase::Developer,"actual native exception propagates and restores parent timing");
        memory.uid_read=false;error.clear();const auto main_before=count(t::Part::WorldPropMainMember);
        need(!reader.main_member(1,2,3,error)&&error=="retained WorldProp read failure","actual main-member read cause unchanged");
        need(count(t::Part::WorldPropMainMember)==main_before+1&&t::query_phase==t::QueryPhase::Developer,"actual main-member early return closes timer");
        world.origin.map=reinterpret_cast<void*>(uintptr_t(0x5000));error.clear();const auto root_before=count(t::Part::WorldPropRootRegion);
        need(!reader.root(world,1,error)&&error=="retained WorldProp read failure","actual root-list read cause unchanged");
        need(count(t::Part::WorldPropRootRegion)==root_before+1&&t::page_read_scope==nullptr&&t::query_phase==t::QueryPhase::Developer,"actual root return closes timing and metadata scope");
    }
    need(t::take_depth==0&&t::query_phase==t::QueryPhase::Other,"WorldProp measurement restores original context");
    const auto data=t::snapshot();
    for(const char* name:{"world_prop_current","world_prop_typed_lookup","world_prop_root_region","world_prop_main_member"})
        need(data.contains(name)&&data.at(name).contains("count")&&data.at(name).contains("total_ns")&&data.at(name).contains("max_ns"),"four WorldProp scopes use existing diagnostic serialization");
    t::totals={};
}
