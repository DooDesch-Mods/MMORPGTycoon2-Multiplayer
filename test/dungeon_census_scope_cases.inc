#include <mutex>
namespace dungeon_census_scope_fixture {
namespace identity {
struct Scope {int revision=0;bool operator==(const Scope&)const=default;};
struct Member {uint32_t value=0;bool operator==(const Member&)const=default;};
inline bool registry_ok=true;inline size_t checks=0;
struct Registry {bool preview=false;bool verify_roster(const Scope&,const std::vector<Member>& members,std::string& e)const{
    need(!t::PageReadScope::reader_active(),"Dungeon final registry check entered reader scope");++checks;
    if(!registry_ok){e=preview?"fixture preview registry rejected":"fixture registry rejected";return false;}
    need(members.size()==1,"Dungeon final complete member list changed");return true;
}};
}
struct World {int revision=1;};struct Exterior {};
struct Census {struct Saved {identity::Member member;};std::vector<Saved> saved;};
inline identity::Registry registry;inline std::mutex gate;
struct State {
    Fixture f;Memory memory;bool exterior_ok=true,census_ok=true,world_ok=true,world_drift=false,callbacks=false;
    uint32_t native_error=0,cast_error=0;size_t exterior_calls=0,census_calls=0,native_calls=0;std::function<void()> native_visit;
};
inline State* active=nullptr;
bool fail(std::string& e,const std::string& reason){e=reason;return false;}
identity::Scope scope(const World& value){return {value.revision};}
bool current(identity::Scope& out,std::string& e){need(!t::PageReadScope::reader_active(),"Dungeon final World check entered reader scope");
    if(!active->world_ok)return fail(e,"fixture World rejected");out={active->world_drift?2:1};return true;
}
uint32_t original_find(int,uint64_t,uintptr_t& found){need(!t::PageReadScope::reader_active(),"Dungeon FindUID failed to invalidate metadata");
    ++active->native_calls;if(active->native_visit)active->native_visit();found=active->f.base;return active->native_error;
}
uint32_t original_cast(int,uintptr_t object,uintptr_t,uintptr_t,uintptr_t& found){need(!t::PageReadScope::reader_active(),"Dungeon RTTI failed to invalidate metadata");
    ++active->native_calls;if(active->native_visit)active->native_visit();found=object;return active->cast_error;
}
bool current_exteriors(World& world,std::vector<Exterior>& exteriors,std::string& e){
    t::PageReadScope pages(!t::PageReadScope::reader_active());++active->exterior_calls;world={};exteriors.clear();
    if(!active->exterior_ok)return fail(e,"fixture exterior reader rejected");
    uint32_t value=0;for(size_t i=0;i<32;++i)if(!active->memory.read(active->f.base+i*4,&value,sizeof value,e))return false;
    return true;
}
bool read_census(const World&,const std::vector<Exterior>&,bool snapshot,Census& out,std::string& e,const identity::Registry*){
    t::PageReadScope pages(!t::PageReadScope::reader_active());++active->census_calls;need(!snapshot,"Dungeon current census changed snapshot mode");
    if(!active->census_ok)return fail(e,"fixture census reader rejected");
    struct Api {int find=0,cast=0;};const Api api;const uintptr_t image=active->f.base;
#include "dungeon_census_native_callbacks.inc"
    if(active->callbacks){uintptr_t found=0;uint32_t observed=0;
        if(!find(7,found,e)||!active->memory.read(active->f.base,&observed,sizeof observed,e)||!typed(found,0,0,e))return false;}
    uint32_t value=0;for(size_t i=0;i<32;++i)if(!active->memory.read(active->f.base+i*4,&value,sizeof value,e))return false;
    out.saved.push_back({{value}});return true;
}
namespace baseline {
#include "dungeon_census_scope_baseline.inc"
}
namespace actual {
#include "dungeon_census_scope_actual.inc"
}
void run(){
    State state;active=&state;std::memset(state.f.allocation,0,65536);state.f.put(state.f.base+31*4,uint32_t(81));
    t::Scope measured(t::Part::HostCapture);t::PageReadScope original(t::PageBoundary::OriginalCall);auto* parent=t::page_read_scope;
    Census expected,got;std::string e;identity::Registry preview{true};
    const auto check=[&](bool success,bool verify=true,const identity::Registry* supplied=nullptr){
        state.memory.reads.clear();state.exterior_calls=state.census_calls=state.native_calls=identity::checks=0;e.clear();auto start=queries();
        const bool old=baseline::current_census(expected,verify,e,supplied);const auto unscoped=queries()-start;
        const auto reads=state.memory.reads;const auto cause=e;const std::array<size_t,4> calls{state.exterior_calls,state.census_calls,state.native_calls,identity::checks};
        state.memory.reads.clear();state.exterior_calls=state.census_calls=state.native_calls=identity::checks=0;e.clear();start=queries();
        const bool result=actual::current_census(got,verify,e,supplied);const auto scoped=queries()-start;
        need(old==success&&result==old&&e==cause&&state.memory.reads==reads&&calls==std::array<size_t,4>{state.exterior_calls,state.census_calls,state.native_calls,identity::checks},"Dungeon outer scope changed reads/callbacks/failure propagation");
        need(expected.saved.size()==got.saved.size()&&(got.saved.empty()||expected.saved[0].member==got.saved[0].member),"Dungeon outer scope changed published/failed census");
        need(t::page_read_scope==parent&&!t::PageReadScope::reader_active(),"Dungeon outer scope leaked into caller");return std::pair{unscoped,scoped};
    };
    const auto counts=check(true);need(counts.first==2&&counts.second==1,"Dungeon sibling readers did not share permission metadata");
    state.f.put(state.f.base+31*4,uint32_t(82));check(true);need(got.saved[0].member.value==82,"Dungeon census reused previous field value");
    state.callbacks=true;check(true);need(state.native_calls==2,"Dungeon native find/RTTI boundaries were skipped");
    state.native_error=123;check(false);state.native_error=0;state.cast_error=456;check(false);state.cast_error=0;state.callbacks=false;
    state.exterior_ok=false;check(false);state.exterior_ok=true;state.census_ok=false;check(false);state.census_ok=true;
    state.world_ok=false;check(false);state.world_ok=true;state.world_drift=true;check(false);state.world_drift=false;
    identity::registry_ok=false;check(false);check(false,true,&preview);check(true,false);identity::registry_ok=true;check(true,true,&preview);
    state.callbacks=true;
    for(size_t revoke_at:{size_t(1),size_t(2)}){DWORD previous=0;bool changed=false;state.native_calls=0;
        state.native_visit=[&]{if(state.native_calls==revoke_at){need(VirtualProtect(state.f.allocation,65536,PAGE_NOACCESS,&previous)!=0,"Dungeon native protection change failed");changed=true;}};
        const bool accepted=actual::current_census(got,true,e);DWORD ignored=0;
        need(VirtualProtect(state.f.allocation,65536,previous,&ignored)!=0,"Dungeon native page restore failed");
        need(changed&&!accepted&&e=="record fixture unreadable","Dungeon outer reader retained permissions across native callbacks");
    }
    state.native_visit={};state.callbacks=false;check(true);
    std::cout<<"Dungeon outer census: identical sibling reads/errors, OS queries "<<counts.first<<" -> "<<counts.second<<"; extracted UID/RTTI callback invalidation retained\n";active=nullptr;
}
}
