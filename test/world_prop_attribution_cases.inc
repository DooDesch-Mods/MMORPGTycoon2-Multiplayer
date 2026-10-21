#include <algorithm>
#include "object_identity/remove_runtime/copy/saved_scenery_batch.h"
#include <cstring>
#include <set>
#include <vector>
namespace world_prop_attribution_fixture {
struct Memory {
    bool uid_read=false,field_mode=false;
    uintptr_t failed_field=0,throw_field=0;
    std::vector<std::pair<uintptr_t,size_t>> field_reads;
    bool copy(uintptr_t address,void* out,size_t bytes,std::string& e){
        if(field_mode){
            field_reads.emplace_back(address,bytes);
            if(address==throw_field)throw std::runtime_error("original field read exception");
            if(address==failed_field){e="original native field read failure";return false;}
            MEMORY_BASIC_INFORMATION information{};
            if(!t::virtual_query(reinterpret_cast<void*>(address),&information,sizeof information,t::Part::QueryWorldProp)||information.State!=MEM_COMMIT||(information.Protect&(PAGE_NOACCESS|PAGE_GUARD))){e="original native field page unreadable";return false;}
            std::memcpy(out,reinterpret_cast<void*>(address),bytes);return true;
        }
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
    bool throws=false,success=false;
    CurrentWorld value;
    t::PageReadScope* expected_parent=nullptr;
    bool current(CurrentWorld& out,std::string& e){
        need(t::page_read_scope==expected_parent,"native Origin must run outside the new pure-field extent");
        if(success){out=value;return true;}
        if(throws)throw std::runtime_error("retained WorldProp origin exception");
        e="retained WorldProp origin failure";return false;
    }
};
using FindUid=void*(*)(uint64_t);
using DynamicCast=void*(*)(void*,const void*,const void*,ptrdiff_t);
struct LookupApi {FindUid find=nullptr;DynamicCast cast=nullptr;};
inline bool lookup_throws=false,lookup_missing=false;
inline uintptr_t lookup_object=0x4321;
void* find(uint64_t){if(lookup_throws)throw std::runtime_error("retained WorldProp lookup exception");return lookup_missing?nullptr:reinterpret_cast<void*>(lookup_object);}
void* cast(void* value,const void*,const void*,ptrdiff_t){return value;}
namespace opened {using CurrentWorld=world_prop_attribution_fixture::CurrentWorld;}
#include "saved_root_origin_same.inc"
#include "saved_root_batch_lifetime_type.inc"
class Reader;
inline Reader* final_reader=nullptr;
struct RootSceneryReadBatch::State {
    mt2mp::build_actions::private_native::host::SavedSceneryBatch proof;
    WorldState expected;
    uintptr_t image=0;uint32_t thread=0;LookupApi api;
    bool world_ready=false,fresh_world=false;
#include "saved_root_world_admit.inc"
    bool finish(std::string&);
};
#include "saved_root_batch_lifetime_body.inc"
#include "saved_root_world_reuse.inc"
class Reader {public:
    Memory& memory_;Origin& origin_;uintptr_t image_=1;uint32_t thread_=GetCurrentThreadId();LookupApi api_{find,cast};bool world_busy_=false;
    bool current(WorldState&,bool,std::string&);
    bool baseline_current(WorldState&,bool,std::string&);
    bool admit_saved_world(const WorldState&,std::string&);
    bool typed(uintptr_t,uint64_t,uintptr_t,std::string&);
    bool main_member(uintptr_t,uintptr_t,uintptr_t,std::string&);
    bool root(const WorldState&,uintptr_t,std::string&);
};
#include "world_prop_attribution_bodies.inc"
bool Reader::admit_saved_world(const WorldState& world,std::string& error){
    auto* batch=RootSceneryReadBatch::current_?RootSceneryReadBatch::current_->state_.get():nullptr;
    if(!batch){error="fixture requires current saved Root batch";return false;}
    if(!batch->admit(world,image_,thread_,api_,error))return false;
#include "saved_root_world_ready.inc"
    return true;
}
bool RootSceneryReadBatch::State::finish(std::string& error){
    if(!proof.healthy(error))return false;
    if(!proof.active()||!final_reader)return true;
#include "saved_root_world_fresh.inc"
    WorldState before,after;
    if(!final_reader->current(before,true,error)||!admit(before,image,thread,api,error)||
       !final_reader->current(after,true,error)||!admit(after,image,thread,api,error))return proof.reject(error);
    return true;
}
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

void world_context_cases(world_prop_attribution_fixture::Reader& reader,world_prop_attribution_fixture::Memory& memory,
    world_prop_attribution_fixture::Origin& origin,const world_prop_attribution_fixture::WorldState& expected,std::string& error){
    namespace f=world_prop_attribution_fixture;
    f::WorldState actual;
    {f::RootSceneryReadBatch batch;memory.field_reads.clear();
        need(reader.current(actual,true,error)&&memory.field_reads.size()==12,"unadmitted batch performs complete local World field proof");
        memory.field_reads.clear();need(reader.current(actual,true,error)&&memory.field_reads.size()==12,"Buildings-only batch does not cache local World fields without full saved-member admission");
    }
    const auto object=reinterpret_cast<uintptr_t>(expected.origin.game)+0x800;const uint64_t uid=77;
    std::memcpy(reinterpret_cast<void*>(object+0x18),&uid,sizeof uid);f::lookup_object=object;
    {f::RootSceneryReadBatch batch;need(reader.admit_saved_world(expected,error),"admit exact successfully proved saved Root context");
        memory.field_reads.clear();
        for(unsigned i=0;i<100;++i)need(reader.current(actual,true,error)&&reader.typed(object,uid,0x164e5c0,error),"admitted complete World reuse retains each actual typed UID lookup/read");
        need(memory.field_reads.size()==100&&std::all_of(memory.field_reads.begin(),memory.field_reads.end(),[&](const auto& row){return row.first==object+0x18;}),"100 cached World contexts omit1200 repeated global fields and retain all100 fresh object UID reads");
        f::final_reader=&reader;memory.field_reads.clear();need(batch.finish(error)&&memory.field_reads.size()==24,"actual fresh final-context guard forces both complete local World reads despite cache admission");f::final_reader=nullptr;
    }
    memory.field_reads.clear();need(reader.current(actual,true,error)&&memory.field_reads.size()==12,"RAII end restores complete fresh World field reads");
    {f::RootSceneryReadBatch batch;need(reader.admit_saved_world(expected,error),"native boundary context setup failed");
        {t::PageReadScope original(t::PageBoundary::OriginalCall);}
        error.clear();memory.field_reads.clear();need(!reader.current(actual,true,error)&&error=="saved Scenery batch native boundary or owner changed"&&memory.field_reads.empty(),"actual OriginalCall invalidates copied complete local World before reuse");
    }
    for(unsigned kind=0;kind<4;++kind){f::RootSceneryReadBatch batch;need(reader.admit_saved_world(expected,error),"resolver context setup failed");
        const auto image=reader.image_;const auto api=reader.api_;const auto sha=origin.value.sha256;
        if(kind==0)++reader.image_;if(kind==1)reader.api_.find=reinterpret_cast<f::FindUid>(uintptr_t(1));if(kind==2)reader.api_.cast=reinterpret_cast<f::DynamicCast>(uintptr_t(2));if(kind==3)origin.value.sha256=std::string(64,'b');
        error.clear();memory.field_reads.clear();const bool okay=reader.current(actual,true,error);
        reader.image_=image;reader.api_=api;origin.value.sha256=sha;
        need(!okay&&memory.field_reads.empty()&&error.find(kind==3?"cached World origin differs":"complete World or resolver changed")!=std::string::npos,"cached complete World rejects different image/API/origin before publication");
    }
    {f::RootSceneryReadBatch batch;need(reader.admit_saved_world(expected,error),"final local World change setup failed");
        const auto edits=reader.image_+0x13beda0;const int32_t active=1,zero=0;std::memcpy(reinterpret_cast<void*>(edits),&active,sizeof active);f::final_reader=&reader;
        error.clear();const bool okay=batch.finish(error);std::memcpy(reinterpret_cast<void*>(edits),&zero,sizeof zero);f::final_reader=nullptr;
        need(!okay&&error=="info: remove_wait_save_edit_or_graph","fresh final local World rejects changed native idle state despite admitted cache");
    }
    f::lookup_object=0x4321;memory.field_reads.clear();error.clear();
}
