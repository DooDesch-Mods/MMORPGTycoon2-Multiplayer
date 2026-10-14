#include "native_building_catalog.h"
#include "mt2mp/game/native_engine.h"
#include <algorithm>
#include <cstring>
#include <limits>
#include <sstream>
#include <unordered_set>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace mt2mp::game::native_catalog {
namespace {
constexpr uintptr_t map_slot=0x13df8c0, shard_manager_slot=0x13df440;
constexpr uint32_t max_regions=16384, max_slots=1000000;
// Vtable constants point to the first virtual function, after the Itanium header.
constexpr uintptr_t map_vt=0x1742860, region_vt=0x1787930, shard_vt=0x174ad20;
constexpr uintptr_t manager_vt=0x17183c0, array_vt=0x172d7c0, shard_manager_vt=0x1702b00;
struct Backend {
    uintptr_t base=0;
    void* user=nullptr;
    bool (*guard)(void*,std::string&)=nullptr;
    bool (*read)(void*,uintptr_t,void*,size_t)=nullptr;
    bool (*count)(void*,uintptr_t,int32_t&,std::string&)=nullptr;
    bool (*lookup)(void*,uint64_t,uintptr_t&,std::string&)=nullptr;
    bool (*building)(void*,uint64_t,native::BuildingState&,std::string&)=nullptr;
};
template<class T> bool read(const Backend& b,uintptr_t object,size_t offset,T& result) {
    return object && object <= (std::numeric_limits<uintptr_t>::max)()-offset &&
        b.read(b.user,object+offset,&result,sizeof(T));
}
bool vt(const Backend& b,uintptr_t object,uintptr_t expected) {
    uintptr_t actual=0; return read(b,object,0,actual) && actual==b.base+expected;
}
std::string image_rva(const Backend& b,uintptr_t value) {
    if(value<b.base||value-b.base>=0x2000000) return "outside_image";
    std::ostringstream text;text<<"0x"<<std::hex<<(value-b.base);return text.str();
}
bool reject(State& state,const char* reason,const std::string& detail={}) {
    ++state.root.rejected;
    auto found=std::find_if(state.reasons.begin(),state.reasons.end(),[&](const auto& row){return row.reason==reason;});
    if(found==state.reasons.end()) state.reasons.push_back({reason,1}); else ++found->count;
    if(state.first_error.empty()) state.first_error=std::string(reason)+(detail.empty()?"":" "+detail);
    return false;
}
bool prologues(const Backend& b,State& state) {
    struct Guard {uintptr_t rva; const char* bytes; size_t size;};
    const Guard guards[]={
        {0x24d740,"\x8b\x81\x18\x04\x00\x00\xc3",7},
        {0x24d7e0,"\x56\x53\x48\x83\xec\x28\x48\x63\xda\x48\x89\xce\x85\xdb\x78\x08",16},
        {0x24e9fa,"\x8b\x81\x18\x04\x00\x00\x48\x89\x91\x50\x04\x00\x00\x48\x89\xce\x4c\x89\x81\x58\x04\x00\x00",23},
        {0x356e89,"\x48\x8b\x8b\x50\x09\x00\x00",7},
        {0x509ce1,"\x48\x89\x1d\x58\x57\xed\x00",7},
        {0x509d1e,"\x48\x8d\x05\xdb\x8d\x1f\x01",7},
        {0x509d39,"\x48\x89\x03",3},
        {0x47ce20,"\x53\x45\x31\xc0\x48\x85\xc9\x74\x31\x48\x83\x3d\xa7\x95\x5b\x01",16},
        {0x12654f0,"\x41\x54\x55\x57\x56\x53\x48\x83\xec\x70\x48\x89\xcb\x48\x89\xd5",16}};
    unsigned char bytes[32]{};
    for(const auto& g:guards) {
        if(!b.read(b.user,b.base+g.rva,bytes,g.size)||std::memcmp(bytes,g.bytes,g.size))
            return reject(state,"prologue_mismatch","rva="+std::to_string(g.rva));
    }
    return true;
}
bool scan(const Backend& b,State& state,std::string& error) {
    state={}; error.clear();
    std::string cause;
    auto finish=[&] {
        state.scan_complete=state.root.observed && state.root.rejected==0;
        state.root.scan_complete=state.scan_complete;
        error=state.first_error;
        return state.scan_complete;
    };
    if(!b.guard(b.user,cause)) {reject(state,"world_thread_build_guard",cause); return finish();}
    if(!prologues(b,state)) return finish();
    uintptr_t map=0,shard_manager=0,root_shard=0,actual_vtable=0;
    if(!read(b,b.base,map_slot,map)||!vt(b,map,map_vt)) {reject(state,"map_receiver_invalid");return finish();}
    if(!read(b,b.base,shard_manager_slot,shard_manager)) {reject(state,"shard_manager_singleton_unreadable");return finish();}
    if(!shard_manager) {reject(state,"shard_manager_missing");return finish();}
    if(!read(b,shard_manager,0,actual_vtable)) {reject(state,"shard_manager_vtable_unreadable");return finish();}
    if(actual_vtable!=b.base+shard_manager_vt) {reject(state,"shard_manager_vtable_mismatch","expected_image_rva=0x1702b00 actual_image_rva="+image_rva(b,actual_vtable));return finish();}
    if(!read(b,shard_manager,0x60,root_shard)) {reject(state,"view_shard_unreadable");return finish();}
    if(!root_shard) {reject(state,"view_shard_missing");return finish();}
    if(!read(b,root_shard,0,actual_vtable)) {reject(state,"view_shard_vtable_unreadable");return finish();}
    if(actual_vtable!=b.base+shard_vt) {reject(state,"view_shard_vtable_mismatch","expected_image_rva=0x174ad20 actual_image_rva="+image_rva(b,actual_vtable));return finish();}
    int32_t root_type=-1;
    if(!read(b,root_shard,0x48,root_type)) {reject(state,"view_shard_type_unreadable");return finish();}
    if(root_type!=0) {reject(state,"root_shard_type_not0","actual_type="+std::to_string(root_type)+" required_type=0 root_map_view");return finish();}
    uintptr_t regions=0; int32_t count=-1,capacity=-1;
    if(!read(b,map,0x358,regions)||!read(b,map,0x360,count)||!read(b,map,0x364,capacity)||!regions||count<=0||count>int32_t(max_regions)||capacity<count) {
        reject(state,"region_array_bounds","count="+std::to_string(count)+" capacity="+std::to_string(capacity)+" required=1..16384 and capacity>=count");return finish();
    }
    state.root.observed=true; state.root.regions=uint32_t(count);
    std::unordered_set<uintptr_t> seen_regions,seen_managers,seen_buildings;
    std::unordered_set<uint64_t> seen_uids;
    for(int32_t region_index=0;region_index<count;++region_index) {
        const auto label="region_index="+std::to_string(region_index);
        uintptr_t region=0,manager=0,manager_shard=0,manager_region=0;
        if(!read(b,regions,size_t(region_index)*8,region)||!vt(b,region,region_vt)) {reject(state,"region_receiver_invalid",label);continue;}
        if(!seen_regions.insert(region).second) {reject(state,"duplicate_region",label);continue;}
        if(!read(b,region,0x950,manager)||!vt(b,manager,manager_vt)) {reject(state,"manager_receiver_invalid",label);continue;}
        if(!seen_managers.insert(manager).second) {reject(state,"duplicate_manager",label);continue;}
        if(!read(b,manager,0x450,manager_shard)||manager_shard!=root_shard) {reject(state,"manager_root_shard_mismatch",label);continue;}
        if(!read(b,manager,0x458,manager_region)||manager_region!=region) {reject(state,"manager_reverse_region_mismatch",label);continue;}
        if(!vt(b,manager+0x408,array_vt)) {reject(state,"main_array_vtable_mismatch",label);continue;}
        ++state.root.managers;
        uintptr_t slots=0; int32_t length=-1,allocated=-1,native_count=-1;
        if(!read(b,manager,0x410,slots)||!read(b,manager,0x418,length)||!read(b,manager,0x41c,allocated)||length<0||allocated<length||
            uint64_t(length)>max_slots||state.root.native_main_slots+uint32_t(length)>max_slots||(length&&!slots)) {
            reject(state,"main_array_bounds",label+" count="+std::to_string(length)+" capacity="+std::to_string(allocated)+" required=0..1000000 total<=1000000 and capacity>=count");continue;
        }
        cause.clear();
        if(!b.count(b.user,manager,native_count,cause)) {reject(state,"native_count_failed",label+" "+cause);continue;}
        if(native_count!=length) {reject(state,"native_count_mismatch",label+" native="+std::to_string(native_count)+" array="+std::to_string(length));continue;}
        state.root.native_main_slots+=uint32_t(length);
        for(int32_t index=0;index<length;++index) {
            const auto slot_label=label+" slot="+std::to_string(index);
            uintptr_t building=0;
            if(!read(b,slots,size_t(index)*8,building)) {reject(state,"slot_unreadable",slot_label);continue;}
            if(!building) {++state.root.null_slots;reject(state,"null_slot",slot_label);continue;}
            ++state.root.non_null;
            uint64_t uid=0; uintptr_t lookup=0,actual_region=0,actual_shard=0;
            if(!read(b,building,0x18,uid)||!uid) {reject(state,"uid_unreadable_or_zero",slot_label);continue;}
            if(!seen_buildings.insert(building).second||!seen_uids.insert(uid).second) {++state.root.duplicates;reject(state,"duplicate_building_uid",slot_label+" uid="+std::to_string(uid));continue;}
            cause.clear();
            if(!b.lookup(b.user,uid,lookup,cause)) {reject(state,"typed_uid_lookup_failed",slot_label+" "+cause);continue;}
            if(lookup!=building) {reject(state,"typed_uid_registry_mismatch",slot_label+" uid="+std::to_string(uid));continue;}
            if(!read(b,building,0x170,actual_shard)||actual_shard!=root_shard) {reject(state,"building_shard_mismatch",slot_label);continue;}
            if(!read(b,building,0x178,actual_region)||actual_region!=region) {reject(state,"building_region_mismatch",slot_label);continue;}
            native::BuildingState row;
            if(!b.building(b.user,uid,row,cause)) {reject(state,"native_building_read_rejected",slot_label+" "+cause);continue;}
            if(row.prop.uid!=uid||row.dungeon_membership||row.dungeon_instance_membership) {reject(state,"native_building_membership_mismatch",slot_label);continue;}
            uintptr_t final_slot=0,final_region=0,final_shard=0;uint64_t final_uid=0;
            if(!read(b,slots,size_t(index)*8,final_slot)||final_slot!=building||!read(b,building,0x178,final_region)||final_region!=region||
                !read(b,building,0x170,final_shard)||final_shard!=root_shard||!read(b,building,0x18,final_uid)||final_uid!=uid) {reject(state,"building_changed_during_read",slot_label);continue;}
            ++state.root.typed_valid;
            if(state.samples.size()<32) state.samples.push_back({uid,uint32_t(region_index),false,0,"root",row.type,row.variant});
        }
        uintptr_t after_slots=0,after_region=0,after_shard=0,after_manager=0; int32_t after_length=-1,after_capacity=-1;
        if(!read(b,manager,0x410,after_slots)||after_slots!=slots||!read(b,manager,0x418,after_length)||after_length!=length||
            !read(b,manager,0x41c,after_capacity)||after_capacity!=allocated||!read(b,manager,0x458,after_region)||after_region!=region||
            !read(b,manager,0x450,after_shard)||after_shard!=root_shard||!read(b,region,0x950,after_manager)||after_manager!=manager)
            reject(state,"manager_changed_during_read",label);
    }
    uintptr_t final_map=0,final_shard_manager=0,final_root=0,final_regions=0; int32_t final_count=-1,final_capacity=-1;
    if(!read(b,b.base,map_slot,final_map)||final_map!=map||!read(b,b.base,shard_manager_slot,final_shard_manager)||final_shard_manager!=shard_manager||
        !read(b,shard_manager,0x60,final_root)||final_root!=root_shard||!read(b,map,0x358,final_regions)||final_regions!=regions||
        !read(b,map,0x360,final_count)||final_count!=count||!read(b,map,0x364,final_capacity)||final_capacity!=capacity)
        reject(state,"world_changed_during_read");
    cause.clear(); if(!b.guard(b.user,cause)) reject(state,"final_world_thread_build_guard",cause);
    return finish();
}
#ifndef MT2MP_CATALOG_TEST
thread_local DWORD first_read_exception=0;
bool process_read(void*,uintptr_t address,void* output,size_t size) {
    if(!address||!size||address>(std::numeric_limits<uintptr_t>::max)()-size) return false;
    auto current=address;const auto end=address+size;
    while(current<end) {
        MEMORY_BASIC_INFORMATION m{};
        if(!VirtualQuery(reinterpret_cast<const void*>(current),&m,sizeof(m))||m.State!=MEM_COMMIT||(m.Protect&(PAGE_GUARD|PAGE_NOACCESS))) return false;
        const auto p=m.Protect&0xff;
        if(p!=PAGE_READONLY&&p!=PAGE_READWRITE&&p!=PAGE_WRITECOPY&&p!=PAGE_EXECUTE_READ&&p!=PAGE_EXECUTE_READWRITE&&p!=PAGE_EXECUTE_WRITECOPY) return false;
        const auto start=reinterpret_cast<uintptr_t>(m.BaseAddress);
        if(start>(std::numeric_limits<uintptr_t>::max)()-m.RegionSize||start+m.RegionSize<=current) return false;
        current=std::min(end,start+m.RegionSize);
    }
    __try {std::memcpy(output,reinterpret_cast<const void*>(address),size);return true;}
    __except(EXCEPTION_EXECUTE_HANDLER) {if(!first_read_exception) first_read_exception=GetExceptionCode();return false;}
}
bool process_guard(void*,std::string& error) {
    if(!native::ready()) {error="exact-build resolver unavailable";return false;}
    const auto base=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    if(reinterpret_cast<uintptr_t>(native::entrypoint(native::EntryPoint::GameUpdate))!=base+0x463ce0) {error="resolver module mismatch";return false;}
    native::WorldStatus world;
    if(!native::read_world_status(world,error)) return false;
    if(!native::in_game()||!world.world_present||world.quitting) {error="active world required";return false;}
    return true;
}
DWORD raw_count(uintptr_t manager,int32_t& value) {
    const auto base=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    __try {value=reinterpret_cast<int32_t(__fastcall*)(void*)>(base+0x24d740)(reinterpret_cast<void*>(manager));return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER) {return GetExceptionCode();}
}
DWORD raw_lookup(uint64_t uid,uintptr_t& value) {
    const auto base=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    __try {
        auto* object=reinterpret_cast<void*(__fastcall*)(uint64_t)>(base+0x47ce20)(uid);
        value=object?reinterpret_cast<uintptr_t>(reinterpret_cast<void*(__fastcall*)(void*,const void*,const void*,ptrdiff_t)>(base+0x12654f0)(
            object,reinterpret_cast<void*>(base+0x1673900),reinterpret_cast<void*>(base+0x1655090),0)):0;
        return 0;
    } __except(EXCEPTION_EXECUTE_HANDLER) {return GetExceptionCode();}
}
bool process_count(void*,uintptr_t manager,int32_t& value,std::string& error) {
    const auto code=raw_count(manager,value);if(code) {error="SEH="+std::to_string(code);return false;}return true;
}
bool process_lookup(void*,uint64_t uid,uintptr_t& value,std::string& error) {
    const auto code=raw_lookup(uid,value);if(code) {error="SEH="+std::to_string(code);return false;}return true;
}
bool process_building(void*,uint64_t uid,native::BuildingState& row,std::string& error) {return native::read_building(uid,row,error);}
#endif
}
#ifndef MT2MP_CATALOG_TEST
bool read_native_catalog(State& output,std::string& error) {
    const Backend backend{reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr)),nullptr,process_guard,process_read,process_count,process_lookup,process_building};
    first_read_exception=0;
    try {
        const bool result=scan(backend,output,error);
        if(first_read_exception) {
            error+=" memory_read_SEH="+std::to_string(first_read_exception);
            output.first_error=error;
        }
        return result;
    }
    catch(const std::exception& e) {reject(output,"catalog_exception",e.what());output.scan_complete=false;output.root.scan_complete=false;error=output.first_error;return false;}
}
#endif
}
