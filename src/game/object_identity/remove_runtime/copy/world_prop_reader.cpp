#include "world_prop_reader.h"
#include "saved_scenery_reader.h"
#include "saved_scenery_batch.h"
#include "live_host_state/memory_query_timing.h"
#include <algorithm>
#include <limits>
#include <set>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

namespace mt2mp::build_actions::private_native::host {
namespace {
bool fail(std::string& e,const char* s){if(e.empty())e=s;return false;}
struct Busy {bool& b;explicit Busy(bool& v):b(v){b=true;}~Busy(){b=false;}};
template<class T>bool at(Memory& m,uintptr_t p,size_t offset,T& v,std::string& e){
    if(!p||p>UINTPTR_MAX-offset)return fail(e,"remove_reader_address_range");
    if(!m.copy(p+offset,&v,sizeof(v),e)){if(e.empty())e="remove_reader_memory_unreadable";return false;}return true;
}
bool vt(Memory& m,uintptr_t image,uintptr_t object,uintptr_t expected,std::string& e){
    uintptr_t actual=0;if(!at(m,object,0,actual,e))return false;
    if(actual!=image+expected){e="remove_reader_vtable expected_rva="+std::to_string(expected);return false;}return true;
}
bool array(Memory& m,uintptr_t data,int32_t count,int32_t capacity,std::vector<uintptr_t>& out,std::string& e){
    if(count<0||capacity<count)return fail(e,"remove_reader_array_count_capacity");
    const auto bytes=static_cast<size_t>(count)*sizeof(uintptr_t);
    if(bytes&&(!data||data>UINTPTR_MAX-bytes))return fail(e,"remove_reader_array_range");
    out.clear();std::set<uintptr_t> unique;
    // Bound temporary storage independently of the native count; every slot is still validated.
    uintptr_t values[64]{};
    for(size_t first=0;first<static_cast<size_t>(count);){
        const auto slots=std::min(size_t(64),static_cast<size_t>(count)-first);
        std::string chunk_error;
        const bool copied=m.copy(data+first*sizeof(uintptr_t),values,slots*sizeof(uintptr_t),chunk_error);
        for(size_t i=0;i<slots;++i){
            // A later unreadable slot must not hide an earlier null or duplicate.
            if(!copied&&!m.copy(data+(first+i)*sizeof(uintptr_t),&values[i],sizeof(uintptr_t),e))return false;
            const auto value=values[i];
            if(!value||!unique.insert(value).second)return fail(e,"remove_reader_array_null_or_duplicate");
            out.push_back(value);
        }
        if(!copied){if(e.empty())e=chunk_error;return false;}
        first+=slots;
    }
    return true;
}
bool list(Memory& m,uintptr_t base,size_t off,std::vector<uintptr_t>& out,std::string& e){
    uintptr_t data=0;int32_t count=0,cap=0;
    if(!at(m,base,off,data,e)||!at(m,base,off+8,count,e)||!at(m,base,off+12,cap,e)||!array(m,data,count,cap,out,e))return false;
    uintptr_t after_data=0;int32_t after_count=0,after_cap=0;
    if(!at(m,base,off,after_data,e)||!at(m,base,off+8,after_count,e)||!at(m,base,off+12,after_cap,e))return false;
    return (data==after_data&&count==after_count&&cap==after_cap)||fail(e,"remove_reader_array_changed_during_copy");
}
uint32_t lookup(const LookupApi& api,uintptr_t image,uint64_t uid,uintptr_t source,uintptr_t target,uintptr_t object,uintptr_t& found,uintptr_t& casted){
    __try {found=object?object:reinterpret_cast<uintptr_t>(api.find(uid));
        casted=found?reinterpret_cast<uintptr_t>(api.cast(reinterpret_cast<void*>(found),reinterpret_cast<void*>(image+source),reinterpret_cast<void*>(image+target),0)):0;return 0;}
    __except(GetExceptionCode()==0xe06d7363UL?EXCEPTION_CONTINUE_SEARCH:EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t copy_native(uintptr_t p,void* to,size_t bytes){__try {std::memcpy(to,reinterpret_cast<const void*>(p),bytes);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
}
bool ProcessMemory::copy(uintptr_t address,void* out,size_t bytes,std::string& e){
    if(!address||!out||!bytes||address>UINTPTR_MAX-bytes)return fail(e,"remove_memory_range");
    uintptr_t cursor=address;const uintptr_t end=address+bytes;
    while(cursor<end){MEMORY_BASIC_INFORMATION info{};
        if(!mt2mp::game::live_host_state::apply_timing::virtual_query(reinterpret_cast<void*>(cursor),&info,sizeof(info),mt2mp::game::live_host_state::apply_timing::Part::QueryWorldProp)||info.State!=MEM_COMMIT||(info.Protect&(PAGE_NOACCESS|PAGE_GUARD)))return fail(e,"remove_memory_page_unreadable");
        const auto start=reinterpret_cast<uintptr_t>(info.BaseAddress);
        if(start>UINTPTR_MAX-info.RegionSize||start+info.RegionSize<=cursor)return fail(e,"remove_memory_page_range");
        cursor=std::min(end,start+info.RegionSize);
    }
    const auto code=copy_native(address,out,bytes);if(code){e="remove_memory_SEH_"+std::to_string(code);return false;}return true;
}
bool resolve_lookup(Memory& m,uintptr_t image,std::string_view hash,LookupApi& api,std::string& e){
    api={};if(hash!=mt2mp::native_uid::required_exe_sha)return fail(e,"remove_lookup_exe_hash");
    if(!image||image>UINTPTR_MAX-0x1a3aa88)return fail(e,"remove_lookup_image_range");
    const std::pair<uintptr_t,std::vector<uint8_t>> guards[]={
        {0x47ce20,{0x53,0x45,0x31,0xc0,0x48,0x85,0xc9,0x74,0x31,0x48,0x83,0x3d,0xa7,0x95,0x5b,0x01}},
        {0x12654f0,{0x41,0x54,0x55,0x57,0x56,0x53,0x48,0x83,0xec,0x70,0x48,0x89,0xcb,0x48,0x89,0xd5}}};
    for(const auto& [pc,expected]:guards){std::vector<uint8_t> actual(expected.size());if(!m.copy(image+pc,actual.data(),actual.size(),e))return false;if(actual!=expected){e="remove_lookup_prologue_"+std::to_string(pc);return false;}}
    api={reinterpret_cast<FindUid>(image+0x47ce20),reinterpret_cast<DynamicCast>(image+0x12654f0)};return true;
}
bool same_world(const opened::CurrentWorld& a,const opened::CurrentWorld& b){return a.game==b.game&&a.mode==b.mode&&a.state==b.state&&a.world==b.world&&a.map==b.map&&a.generation==b.generation&&a.world_epoch==b.world_epoch&&a.uuid==b.uuid&&a.sha256==b.sha256&&a.bytes==b.bytes;}
bool Reader::current(WorldState& out,bool idle,std::string& e){
    mt2mp::game::live_host_state::apply_timing::Scope measured(mt2mp::game::live_host_state::apply_timing::Part::WorldPropCurrent);
    out={};e.clear();if(world_busy_)return fail(e,"remove_world_reader_reentrant");Busy busy(world_busy_);
    if(!thread_||GetCurrentThreadId()!=thread_)return fail(e,"remove_world_wrong_thread");
    if(!image_||!api_.find||!api_.cast)return fail(e,"remove_world_resolver_missing");
    if(!origin_.current(out.origin,e))return false;
    bool reused=false;
    if(!RootSceneryReadBatch::reuse_world(out,idle,image_,thread_,api_,reused,e))return false;
    if(reused)return true;
    mt2mp::game::live_host_state::apply_timing::PageReadScope fields(!mt2mp::game::live_host_state::apply_timing::PageReadScope::reader_active());
    const auto& o=out.origin;
    if(!o.world_epoch||!o.generation||!o.bytes||o.uuid.empty()||o.sha256.size()!=64)return fail(e,"remove_world_origin_incomplete");
    const auto mode=reinterpret_cast<uintptr_t>(o.mode),game=reinterpret_cast<uintptr_t>(o.game),map=reinterpret_cast<uintptr_t>(o.map);
    uintptr_t v=0;int32_t state=-1;
    if(!vt(memory_,image_,mode,0x16e6730,e)||!vt(memory_,image_,game,0x1743740,e)||!vt(memory_,image_,map,0x1742860,e))return false;
    if(!at(memory_,game,0x28,v,e)||v!=mode)return fail(e,"remove_world_active_mode_changed");
    if(!at(memory_,mode,0x2c4,state,e)||state!=4)return fail(e,"remove_world_not_active_state4");
    if(!at(memory_,mode,0x30c,out.saves,e)||out.saves<0)return fail(e,"remove_world_saves_invalid");
    if(!at(memory_,image_,0x13beda0,out.edits,e)||out.edits<0)return fail(e,"remove_world_edits_invalid");
    if(!at(memory_,image_,0x13df630,out.selection,e)||!vt(memory_,image_,out.selection,0x17205d0,e)||!at(memory_,image_,0x1a3aa80,out.active_graph,e))return false;
    if(map>UINTPTR_MAX-0x1c0)return fail(e,"remove_world_root_overflow");out.root=map+0x1c0;
    if(!vt(memory_,image_,out.root,0x174ad20,e)||!at(memory_,out.root,0x48,state,e)||state!=0)return fail(e,"remove_world_root_type");
    if(idle&&(out.saves||out.edits||out.active_graph))return fail(e,"info: remove_wait_save_edit_or_graph");
    return true;
}
bool Reader::typed(uintptr_t object,uint64_t uid,uintptr_t type,std::string& e){
    uintptr_t found=0,casted=0;uint32_t seh=0;
    {mt2mp::game::live_host_state::apply_timing::Scope measured(mt2mp::game::live_host_state::apply_timing::Part::WorldPropTypedLookup);
        seh=lookup(api_,image_,uid,0x1673900,type,0,found,casted);}
    if(seh){e="remove_prop_lookup_SEH_"+std::to_string(seh);return false;}
    if(found!=object||casted!=object)return fail(e,"remove_prop_registry_or_typed_identity");
    uint64_t actual=0;return at(memory_,object,0x18,actual,e)&&(actual==uid||fail(e,"remove_prop_full_uid_mismatch"));
}
bool Reader::main_member(uintptr_t object,uintptr_t shard,uintptr_t region,std::string& e){
    mt2mp::game::live_host_state::apply_timing::Scope measured(mt2mp::game::live_host_state::apply_timing::Part::WorldPropMainMember);
    uintptr_t manager=0,v=0;if(!at(memory_,region,0x950,manager,e)||!vt(memory_,image_,manager,0x17183c0,e))return false;
    if(!at(memory_,manager,0x450,v,e)||v!=shard)return fail(e,"remove_building_manager_shard");
    if(!at(memory_,manager,0x458,v,e)||v!=region)return fail(e,"remove_building_manager_region");
    if(!vt(memory_,image_,manager+0x408,0x172d7c0,e))return false;
    std::vector<uintptr_t> before,after;if(!list(memory_,manager,0x410,before,e))return false;
    if(std::find(before.begin(),before.end(),object)==before.end())return fail(e,"remove_building_absent_main_array");
    if(!list(memory_,manager,0x410,after,e)||before!=after)return fail(e,"remove_building_main_array_changed");return true;
}
bool Reader::region_member(const WorldState& world,uintptr_t shard,uintptr_t region,std::string& e){
    if(!vt(memory_,image_,region,0x1787930,e)||!vt(memory_,image_,shard,0x174ad20,e))return false;
    uintptr_t actual=0;if(!at(memory_,region,0x70,actual,e)||actual!=shard)return fail(e,"remove_region_shard_mismatch");
    auto root_region=[&](uintptr_t r){
        mt2mp::game::live_host_state::apply_timing::Scope measured(mt2mp::game::live_host_state::apply_timing::Part::WorldPropRootRegion);
        mt2mp::game::live_host_state::apply_timing::PageReadScope pages;
        std::vector<uintptr_t> a,b;if(!list(memory_,reinterpret_cast<uintptr_t>(world.origin.map),0x358,a,e))return false;
        for(auto candidate:a){uintptr_t candidate_shard=0;if(!vt(memory_,image_,candidate,0x1787930,e)||!at(memory_,candidate,0x70,candidate_shard,e)||candidate_shard!=world.root)return fail(e,"remove_root_region_census_owner");}
        if(std::find(a.begin(),a.end(),r)==a.end())return fail(e,"remove_region_absent_root_map");
        if(!list(memory_,reinterpret_cast<uintptr_t>(world.origin.map),0x358,b,e)||a!=b)return fail(e,"remove_root_regions_changed");return true;};
    if(shard==world.root)return root_region(region);
    int32_t type=-1;if(!at(memory_,shard,0x48,type,e))return false;
    uintptr_t dungeon=0,instance=0;
    if(type==1){if(!at(memory_,shard,0x18,dungeon,e))return false;}
    else if(type==2){
        if(!at(memory_,shard,0x20,instance,e)||!vt(memory_,image_,instance,0x1719450,e))return false;
        uintptr_t found=0,casted=0;const auto seh=lookup(api_,image_,0,0x1657270,0x16650e0,instance,found,casted);
        if(seh||casted!=instance)return fail(e,"remove_instance_rtti");
        if(!at(memory_,instance,0x38,actual,e)||actual!=shard)return fail(e,"remove_instance_shard");
        if(!at(memory_,instance,0x40,actual,e)||actual!=region)return fail(e,"remove_instance_region");
        if(!at(memory_,instance,0x30,dungeon,e))return false;
    }else return fail(e,"remove_shard_type_unsupported");
    if(!vt(memory_,image_,dungeon,0x16c0560,e))return false;
    uintptr_t found=0,casted=0;const auto seh=lookup(api_,image_,0,0x1673900,0x164e3d0,dungeon,found,casted);
    if(seh||casted!=dungeon)return fail(e,"remove_dungeon_rtti");
    uintptr_t template_shard=0,template_region=0;
    if(!at(memory_,dungeon,0x48,template_shard,e)||!at(memory_,dungeon,0x50,template_region,e))return false;
    if(type==1&&(template_shard!=shard||template_region!=region))return fail(e,"remove_dungeon_reverse_membership");
    if(type==2){int32_t template_type=-1;
        if(!vt(memory_,image_,template_shard,0x174ad20,e)||!at(memory_,template_shard,0x48,template_type,e)||template_type!=1||!at(memory_,template_shard,0x18,actual,e)||actual!=dungeon)return fail(e,"remove_instance_template_membership");
        std::vector<uintptr_t> a,b;if(!list(memory_,dungeon,0x4c0,a,e)||std::find(a.begin(),a.end(),instance)==a.end())return fail(e,"remove_instance_absent_owner_array");
        if(!list(memory_,dungeon,0x4c0,b,e)||a!=b)return fail(e,"remove_instance_owner_array_changed");
    }
    uintptr_t exterior=0,exterior_region=0;uint64_t uid=0;
    if(!at(memory_,dungeon,0x6f8,exterior,e)||!exterior||!at(memory_,exterior,0x18,uid,e)||!uid||!typed(exterior,uid,0x1655090,e))return false;
    if(!vt(memory_,image_,exterior,0x16d93c0,e)||!at(memory_,exterior,0xa38,actual,e)||actual!=dungeon)return fail(e,"remove_exterior_reverse_dungeon");
    uintptr_t label=0;uint64_t length=0;std::array<char,15> type_label{};
    if(!at(memory_,exterior,0x768,label,e)||!at(memory_,exterior,0x770,length,e)||length!=type_label.size()||!memory_.copy(label,type_label.data(),type_label.size(),e)||std::memcmp(type_label.data(),"DungeonExterior",type_label.size()))return fail(e,"remove_exterior_definition_not_DungeonExterior");
    if(!at(memory_,exterior,0x170,actual,e)||actual!=world.root||!at(memory_,exterior,0x178,exterior_region,e))return fail(e,"remove_exterior_not_root");
    return root_region(exterior_region)&&main_member(exterior,world.root,exterior_region,e);
}
bool Reader::prop(void* receiver,uint64_t expected,mt2mp::prop_lease::Prop& out,std::string& e){
    out={};e.clear();if(busy_)return fail(e,"remove_prop_reader_reentrant");Busy busy(busy_);
    try {
        WorldState before,after;if(!current(before,true,e))return false;
        const auto object=reinterpret_cast<uintptr_t>(receiver);uintptr_t ap=0,shard=0,region=0;uint64_t uid=0;
        if(!at(memory_,object,0,ap,e)||!at(memory_,object,0x18,uid,e)||!uid)return fail(e,"remove_prop_uid_unreadable_or_zero");
        if(expected&&expected!=uid)return fail(e,"remove_prop_expected_full_uid_mismatch");
        const bool building=ap==image_+0x16d93c0,scenery=ap==image_+0x16c1390;
        if(!building&&!scenery){e="remove_prop_class_membership_unimplemented image_rva="+std::to_string(ap>=image_?ap-image_:0);return false;}
        if(!typed(object,uid,building?0x1655090:0x164e5c0,e)||!at(memory_,object,0x170,shard,e)||!at(memory_,object,0x178,region,e)||!region_member(before,shard,region,e))return false;
        if(building){if(!main_member(object,shard,region,e))return false;}
        else {
            struct SceneryAdapter final:place::SceneryHost {Reader& owner;WorldState world;uintptr_t region,shard,manager;LookupApi api;uintptr_t image;
                SceneryAdapter(Reader& o,const WorldState& w,uintptr_t r,uintptr_t s,uintptr_t m,LookupApi a,uintptr_t i):owner(o),world(w),region(r),shard(s),manager(m),api(a),image(i){}
                bool current(place::SceneryContext& c,std::string& err)override{WorldState fresh;if(!owner.current(fresh,true,err)||!same_world(world.origin,fresh.origin))return fail(err,"remove_scenery_world_changed");c={world.origin.world_epoch,region,shard,manager,true,true,true};return true;}
                bool registry_object(uint64_t id,uintptr_t& actual,std::string& err)override{uintptr_t casted=0;const auto code=lookup(api,image,id,0x1673900,0x164e5c0,0,actual,casted);if(code||actual!=casted)return fail(err,"remove_scenery_typed_registry");return true;}
            };
            uintptr_t manager=0;if(!at(memory_,region,0x960,manager,e))return false;
            SceneryAdapter host(*this,before,region,shard,manager,api_,image_);place::SceneryWitness witness;
            if(!place::read_scenery(memory_,host,image_,witness,e))return false;
            if(std::none_of(witness.members.begin(),witness.members.end(),[&](const auto& member){return member.object==object&&member.uid==uid;}))return fail(e,"remove_scenery_absent_manager_census");
        }
        uintptr_t final_ap=0,final_shard=0,final_region=0;
        if(!typed(object,uid,building?0x1655090:0x164e5c0,e)||!at(memory_,object,0,final_ap,e)||final_ap!=ap||!at(memory_,object,0x170,final_shard,e)||final_shard!=shard||!at(memory_,object,0x178,final_region,e)||final_region!=region)return fail(e,"remove_prop_changed_during_read");
        if(!current(after,true,e)||!same_world(before.origin,after.origin))return fail(e,"remove_prop_world_changed");
        out={uid,receiver,true,true,ap,building?"Building/MainArray/globalRootOrDungeon":"Scenery/47CategoryCensus/globalRootOrDungeon"};return true;
    }catch(const std::exception& x){e=std::string("remove_prop_exception: ")+x.what();return false;}
    catch(...){return fail(e,"remove_prop_unknown_exception");}
}
#include "saved_scenery_batch_runtime.inc"

bool Reader::saved_root_scenery(void* receiver,uint64_t uid,int32_t region_id,uintptr_t region,mt2mp::prop_lease::Prop& out,std::string& e){
    out={};e.clear();if(busy_)return fail(e,"remove_prop_reader_reentrant");Busy busy(busy_);
    try{
        WorldState before,after;if(!current(before,true,e))return false;
        auto* batch=RootSceneryReadBatch::current_?RootSceneryReadBatch::current_->state_.get():nullptr;
        if(batch&&!batch->admit(before,image_,thread_,api_,e))return false;
        uintptr_t manager=0;if(!at(memory_,region,0x960,manager,e))return false;
        struct SavedHost final:place::SceneryHost {
            Reader& reader;WorldState expected;uintptr_t region,manager,image;LookupApi api;
            SavedHost(Reader& r,const WorldState& w,uintptr_t p,uintptr_t m,uintptr_t i,LookupApi a):reader(r),expected(w),region(p),manager(m),image(i),api(a){}
            bool current(place::SceneryContext& context,std::string& error)override{
                WorldState world;if(!reader.current(world,true,error))return false;
                if(!same_world(expected.origin,world.origin)||expected.root!=world.root)return fail(error,"saved_scenery_current_world_changed");
                context={world.origin.world_epoch,region,world.root,manager,true,true,true};return true;
            }
            bool registry_object(uint64_t id,uintptr_t& actual,std::string& error)override{
                uintptr_t casted=0;const auto exception=lookup(api,image,id,0x1673900,0x164e5c0,0,actual,casted);
                if(exception){error="saved_scenery_lookup_SEH_"+std::to_string(exception);return false;}
                return actual==casted||fail(error,"saved_scenery_typed_registry_differs");
            }
        } owner(*this,before,region,manager,image_,api_);
        auto exact=[&](uintptr_t object,uint64_t full,std::string& cause){return typed(object,full,0x164e5c0,cause);};
        auto scan_index=[](uintptr_t image,uintptr_t map,uintptr_t root,mt2mp::game::region_saved_identity::Index& index,auto& read,std::string& cause){
            // Share only page metadata during this complete memory-only scan.
            // Original UID/RTTI calls and the Scenery census run after it closes.
            mt2mp::game::live_host_state::apply_timing::PageReadScope pages;
            return mt2mp::game::region_saved_identity::capture(image,map,root,index,read,cause);
        };
        auto capture_index=[&](uintptr_t image,uintptr_t map,uintptr_t root,mt2mp::game::region_saved_identity::Index& index,auto& read,std::string& cause){
            return batch?batch->proof.index(image,map,root,index,read,cause,scan_index):scan_index(image,map,root,index,read,cause);
        };
        auto capture_member=[&](Memory& memory,place::SceneryHost& current_owner,uintptr_t image,uintptr_t current_region,uintptr_t root,uintptr_t current_manager,uintptr_t object,uint64_t full,std::string& cause){
            return batch?batch->proof.member(memory,current_owner,image,current_region,root,current_manager,object,full,cause):
                SavedSceneryMemberCapture{}(memory,current_owner,image,current_region,root,current_manager,object,full,cause);
        };
        mt2mp::prop_lease::Prop value;
        if(!([&]{
            mt2mp::game::live_host_state::apply_timing::PageReadScope fields(batch&&!mt2mp::game::live_host_state::apply_timing::PageReadScope::reader_active());
            return read_saved_root_scenery(memory_,owner,image_,reinterpret_cast<uintptr_t>(before.origin.map),before.root,region_id,region,reinterpret_cast<uintptr_t>(receiver),uid,exact,value,e,capture_index,capture_member);
        }()))return false;
        if(!current(after,true,e))return false;
        if(!same_world(before.origin,after.origin)||before.root!=after.root)return fail(e,"saved_scenery_world_changed_after_read");
        if(batch)batch->world_ready=true;
        out=std::move(value);return true;
    }catch(const std::exception& exception){e=std::string("saved_scenery_read_exception: ")+exception.what();return false;}
    catch(...){return fail(e,"saved_scenery_read_unknown_exception");}
}

}
