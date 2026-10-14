#include "mt2mp/game/native_network.h"
#include "mt2mp/game/native_engine.h"
#include "mt2mp/core/logger.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <mutex>
#include <stdexcept>

namespace mt2mp::game::native_network {
namespace {
constexpr uintptr_t map_slot=0x13df8c0,analytics_slot=0x13df280;
constexpr uintptr_t map_vtable=0x1742860,region_vtable=0x1787930,manager_vtable=0x17141e0,node_vtable=0x16fc700,definition_vtable=0x1738010,analytics_vtable=0x16dd3b0;
constexpr uint32_t max_regions=16384,max_nodes=65536;
std::mutex log_mutex;
std::string last_failure;
uint64_t repeated_failures=0;
#ifdef MT2MP_NETWORK_TEST
uintptr_t test_base=0;
testing::Getter test_map=nullptr,test_region=nullptr,test_extra=nullptr;
#endif
bool fail(std::string& error,const std::string& cause) {
    error=cause;
    std::lock_guard lock(log_mutex);
    if(last_failure!=cause) {
        LOG_ERROR("[NativeNetwork] Read rejected: {}",cause);
        last_failure=cause; repeated_failures=0;
    }
    ++repeated_failures;
    return false;
}
void recover() {
    std::lock_guard lock(log_mutex);
    if(!last_failure.empty()) {
        LOG_INFO("[NativeNetwork] Read recovered previous={} repeated={}",last_failure,repeated_failures);
        last_failure.clear(); repeated_failures=0;
    }
}
bool readable(const void* pointer,size_t bytes) {
    if(!pointer||!bytes) return false;
    const auto start=reinterpret_cast<uintptr_t>(pointer);
    if(start>(std::numeric_limits<uintptr_t>::max)()-bytes) return false;
    auto current=start; const auto end=start+bytes;
    while(current<end) {
        MEMORY_BASIC_INFORMATION memory{};
        if(!VirtualQuery(reinterpret_cast<const void*>(current),&memory,sizeof(memory))||memory.State!=MEM_COMMIT||memory.Protect&(PAGE_GUARD|PAGE_NOACCESS)) return false;
        const auto protection=memory.Protect&0xff;
        if(protection!=PAGE_READONLY&&protection!=PAGE_READWRITE&&protection!=PAGE_WRITECOPY&&protection!=PAGE_EXECUTE_READ&&protection!=PAGE_EXECUTE_READWRITE&&protection!=PAGE_EXECUTE_WRITECOPY) return false;
        const auto region=reinterpret_cast<uintptr_t>(memory.BaseAddress);
        if(region>(std::numeric_limits<uintptr_t>::max)()-memory.RegionSize) return false;
        const auto next=region+memory.RegionSize;
        if(next<=current) return false;
        current=std::min(end,next);
    }
    return true;
}
template<class T> bool read_at(const void* object,size_t offset,T& value) {
    const auto base=reinterpret_cast<uintptr_t>(object);
    if(!object||base>(std::numeric_limits<uintptr_t>::max)()-offset) return false;
    const auto address=reinterpret_cast<const void*>(base+offset);
    if(!readable(address,sizeof(T))) return false;
    std::memcpy(&value,address,sizeof(T)); return true;
}
bool vtable(const void* object,uintptr_t base,uintptr_t expected) {
    uintptr_t actual=0; return read_at(object,0,actual)&&actual==base+expected;
}
uintptr_t module_base() {
#ifdef MT2MP_NETWORK_TEST
    return test_base;
#else
    return reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
#endif
}
bool prologues(uintptr_t base,std::string& error) {
    struct Guard { uintptr_t rva; const char* bytes; size_t size; };
    const Guard guards[]={
        {0x32b670,"\x57\x56\x53\x48\x83\xec\x20\x8b\x81\x60\x03\x00\x00\x48\x89\xce",16},
        {0x34f860,"\x48\x83\xec\x28\x31\xc0\x80\xb9\x50\x08\x00\x00\x00\x74\x24\x83",16},
        {0x2a5140,"\x55\x57\x56\x53\x48\x83\xec\x28\x8b\x51\x28\x48\x89\xcf\x85\xd2",16},
        {0x42cee0,"\x0f\xb6\x81\xea\x02\x00\x00\xc3",8},
        {0x42cef0,"\x53\x48\x83\xec\x20\x48\x83\xb9\x58\x02\x00\x00\x00\x48\x89\xcb",16}};
    for(const auto& guard:guards) {
        const auto address=reinterpret_cast<const void*>(base+guard.rva);
        if(!readable(address,guard.size)||std::memcmp(address,guard.bytes,guard.size)) return fail(error,"network getter prologue mismatch rva="+std::to_string(guard.rva));
    }
    return true;
}
DWORD call_int(uintptr_t base,uintptr_t rva,void* object,int32_t& result) {
    using Getter=int32_t(__fastcall*)(void*);
    auto function=reinterpret_cast<Getter>(base+rva);
#ifdef MT2MP_NETWORK_TEST
    function=rva==0x32b670?test_map:rva==0x34f860?test_region:test_extra;
    if(!function) return ERROR_PROC_NOT_FOUND;
#endif
    __try { result=function(object); return 0; }
    __except(EXCEPTION_EXECUTE_HANDLER) { return GetExceptionCode(); }
}
bool invoke(uintptr_t base,uintptr_t rva,void* object,int32_t& result,std::string& error) {
    const auto code=call_int(base,rva,object,result);
    if(code) return fail(error,"network getter exception rva="+std::to_string(rva)+" SEH="+std::to_string(code));
    return true;
}
bool valid_integer_float(float value,bool nonnegative) {
    return std::isfinite(value)&&(!nonnegative||value>=0)&&double(value)>=(std::numeric_limits<int32_t>::min)()&&double(value)<=(std::numeric_limits<int32_t>::max)();
}
bool bounded_int(int64_t value) {
    return value>=(std::numeric_limits<int32_t>::min)()&&value<=(std::numeric_limits<int32_t>::max)();
}
}
bool read_state(State& output,std::string& error) {
    try {
        if(!native::ready()) return fail(error,"network exact-build resolver unavailable");
        native::WorldStatus world;
        if(!native::read_world_status(world,error)) return fail(error,"network world/thread guard rejected: "+error);
        if(!native::in_game()||!world.world_present||world.quitting) return fail(error,"network read requires active native world");
        const auto base=module_base();
        if(!base||reinterpret_cast<uintptr_t>(native::entrypoint(native::EntryPoint::GameUpdate))!=base+0x463ce0) return fail(error,"network module differs from exact-build native resolver");
        if(!prologues(base,error)) return false;
        void* map=nullptr;
        if(!read_at(reinterpret_cast<void*>(base),map_slot,map)||!vtable(map,base,map_vtable)) return fail(error,"network map singleton/vtable mismatch");
        void** regions=nullptr; int32_t region_count=-1,region_capacity=-1;
        if(!read_at(map,0x358,regions)||!read_at(map,0x360,region_count)||!read_at(map,0x364,region_capacity)||region_count<0||region_count>int32_t(max_regions)||region_capacity<region_count) return fail(error,"network region count actual="+std::to_string(region_count)+" allocated="+std::to_string(region_capacity)+" required=0.."+std::to_string(max_regions)+" and<=allocated");
        if((region_count||region_capacity)&&!readable(regions,std::max(size_t(region_count),size_t(1))*sizeof(void*))) return fail(error,"network region array unreadable");
        State state; state.region_count=uint32_t(region_count);
        int64_t total_base=0,total_extra=0,total_capacity=0;
        for(int32_t index=0;index<region_count;++index) {
            void* region=nullptr;
            if(!read_at(regions,size_t(index)*sizeof(void*),region)||!vtable(region,base,region_vtable)) return fail(error,"network region receiver/vtable mismatch index="+std::to_string(index));
            RegionState row; row.region_index=uint32_t(index);
            uint8_t owned=0; void* manager=nullptr;
            if(!read_at(region,0x850,owned)||owned>1||!read_at(region,0x854,row.type)||!read_at(region,0x978,manager)) return fail(error,"network region fields invalid index="+std::to_string(index));
            row.owned=owned!=0; row.network_manager_present=manager!=nullptr;
            const bool eligible=row.owned&&row.type==1;
            row.base_capacity=eligible?800:0; state.eligible_regions+=eligible;
            int64_t computed_extra=0;
            if(manager) {
                if(!vtable(manager,base,manager_vtable)) return fail(error,"network manager vtable mismatch region="+std::to_string(index));
                void** nodes=nullptr; int32_t count=-1,allocated=-1;
                if(!read_at(manager,0x20,nodes)||!read_at(manager,0x28,count)||!read_at(manager,0x2c,allocated)||count<0||allocated<count||uint32_t(count)>max_nodes||uint64_t(state.node_slots)+uint32_t(count)>max_nodes)
                    return fail(error,"network node count actual="+std::to_string(count)+" allocated="+std::to_string(allocated)+" accumulated="+std::to_string(state.node_slots)+" required<=65536 and<=allocated");
                if(count&&!readable(nodes,size_t(count)*sizeof(void*))) return fail(error,"network node array unreadable region="+std::to_string(index));
                row.node_slots=uint32_t(count); state.node_slots+=row.node_slots;
                for(int32_t node_index=0;node_index<count;++node_index) {
                    void* node=nullptr;
                    if(!read_at(nodes,size_t(node_index)*sizeof(void*),node)) return fail(error,"network node slot unreadable region="+std::to_string(index)+" index="+std::to_string(node_index));
                    if(!node) { ++state.null_node_slots; continue; }
                    if(!vtable(node,base,node_vtable)) return fail(error,"network node vtable mismatch region="+std::to_string(index)+" index="+std::to_string(node_index));
                    void* definition=nullptr; NodeState entry; entry.region_index=uint32_t(index); entry.node_index=uint32_t(node_index);
                    uint8_t provides=0,activated=0;
                    if(!read_at(node,0x258,definition)||!vtable(definition,base,definition_vtable)||
                       !read_at(node,0x250,entry.current)||!read_at(node,0x2a0,entry.last_current)||!read_at(node,0x2e4,entry.startup_timer)||
                       !read_at(node,0x2ea,provides)||provides>1||!read_at(node,0x2ec,activated)||activated>1||
                       !read_at(definition,0xc4,entry.capacity)||!read_at(definition,0xd0,entry.supply))
                        return fail(error,"network node/definition fields invalid region="+std::to_string(index)+" index="+std::to_string(node_index));
                    if(!std::isfinite(entry.current)||!std::isfinite(entry.last_current)||!std::isfinite(entry.startup_timer)||entry.startup_timer<0||
                       !valid_integer_float(entry.capacity,true)||!valid_integer_float(entry.supply,false))
                        return fail(error,"network node numeric domain invalid region="+std::to_string(index)+" index="+std::to_string(node_index));
                    entry.provides_supply=provides!=0; entry.activated=activated!=0;
                    entry.effective_supply=entry.provides_supply?int32_t(entry.supply):0;
                    computed_extra+=entry.effective_supply;
                    if(!bounded_int(computed_extra)) return fail(error,"network extra supply exceeds int32");
                    ++row.node_count; row.active_supply_nodes+=entry.provides_supply;
                    state.active_supply_nodes+=entry.provides_supply; state.activated_nodes+=entry.activated;
                    state.nodes.push_back(entry);
                }
                int32_t actual_extra=0;
                if(!invoke(base,0x2a5140,manager,actual_extra,error)) return false;
                if(actual_extra!=computed_extra) return fail(error,"network extra getter differs from observed nodes region="+std::to_string(index)+" actual="+std::to_string(actual_extra)+" observed="+std::to_string(computed_extra));
            }
            row.extra_supply=eligible?int32_t(computed_extra):0;
            const auto expected_total=int64_t(row.base_capacity)+row.extra_supply;
            if(!bounded_int(expected_total)) return fail(error,"network region capacity exceeds int32");
            if(!invoke(base,0x34f860,region,row.total_capacity,error)) return false;
            if(row.total_capacity!=expected_total) return fail(error,"network region getter differs index="+std::to_string(index)+" actual="+std::to_string(row.total_capacity)+" observed="+std::to_string(expected_total));
            total_base+=row.base_capacity; total_extra+=row.extra_supply; total_capacity+=row.total_capacity;
            if(!bounded_int(total_base)||!bounded_int(total_extra)||!bounded_int(total_capacity)) return fail(error,"network map capacity exceeds int32");
            state.regions.push_back(row);
        }
        void** regions_after=nullptr; int32_t count_after=-1; void* map_after=nullptr;
        if(!read_at(reinterpret_cast<void*>(base),map_slot,map_after)||map_after!=map||!read_at(map,0x358,regions_after)||regions_after!=regions||!read_at(map,0x360,count_after)||count_after!=region_count)
            return fail(error,"network map/region identity changed during read");
        if(!invoke(base,0x32b670,map,state.total_capacity,error)) return false;
        if(state.total_capacity!=total_capacity) return fail(error,"network map getter differs actual="+std::to_string(state.total_capacity)+" observed="+std::to_string(total_capacity));
        state.base_capacity=int32_t(total_base); state.extra_supply=int32_t(total_extra);
        void* analytics=nullptr;
        if(!read_at(reinterpret_cast<void*>(base),analytics_slot,analytics)||!analytics)
            state.analytics_error="analytics singleton unavailable";
        else {
            if(!vtable(analytics,base,analytics_vtable)||!read_at(analytics,0x40,state.analytics_online)||!read_at(analytics,0x48,state.analytics_capacity))
                return fail(error,"network analytics vtable/fields mismatch");
            state.analytics_available=true;
        }
        error.clear(); output=std::move(state); recover(); return true;
    } catch(const std::exception& failure) { return fail(error,std::string("network read exception: ")+failure.what()); }
}
nlohmann::json state_json(const State& state,uint32_t node_limit,uint32_t region_limit) {
    using json=nlohmann::json;
    if(node_limit>max_node_limit) throw std::invalid_argument("network node_limit actual="+std::to_string(node_limit)+" required=0.."+std::to_string(max_node_limit));
    if(region_limit>max_region_limit) throw std::invalid_argument("network region_limit actual="+std::to_string(region_limit)+" required=0.."+std::to_string(max_region_limit));
    const auto node_returned=std::min(state.nodes.size(),size_t(node_limit));
    const auto region_returned=std::min(state.regions.size(),size_t(region_limit));
    json regions=json::array(),nodes=json::array();
    for(size_t index=0;index<region_returned;++index) {
        const auto& region=state.regions[index];
        regions.push_back({{"observation_region_index",region.region_index},{"owned",region.owned},{"type",region.type},
        {"network_manager_present",region.network_manager_present},{"node_slots",region.node_slots},{"node_count",region.node_count},{"active_supply_nodes",region.active_supply_nodes},
        {"base_capacity",region.base_capacity},{"extra_supply",region.extra_supply},{"total_capacity",region.total_capacity}});
    }
    for(size_t index=0;index<node_returned;++index) {
        const auto& node=state.nodes[index];
        nodes.push_back({{"observation_region_index",node.region_index},{"observation_node_index",node.node_index},
        {"current",node.current},{"last_current",node.last_current},{"capacity",node.capacity},{"supply",node.supply},{"startup_timer",node.startup_timer},
        {"provides_supply",node.provides_supply},{"activated",node.activated},{"effective_supply",node.effective_supply}});
    }
    json result={{"base_capacity",state.base_capacity},{"extra_supply",state.extra_supply},{"total_capacity",state.total_capacity},
        {"analytics_available",state.analytics_available},{"analytics_online",state.analytics_available?nlohmann::json(state.analytics_online):nlohmann::json(nullptr)},
        {"analytics_capacity",state.analytics_available?nlohmann::json(state.analytics_capacity):nlohmann::json(nullptr)},{"analytics_error",state.analytics_error},
        {"region_count",state.region_count},{"eligible_regions",state.eligible_regions},{"node_slots",state.node_slots},{"node_count",state.nodes.size()},
        {"node_limit",node_limit},{"region_limit",region_limit},{"node_returned_count",node_returned},{"region_returned_count",region_returned},
        {"nodes_truncated",node_returned<state.nodes.size()},{"regions_truncated",region_returned<state.regions.size()},
        {"null_node_slots",state.null_node_slots},{"active_supply_nodes",state.active_supply_nodes},{"activated_nodes",state.activated_nodes},
        {"serialized_ids_available",state.serialized_ids_available},{"identity_error",state.identity_error},{"regions",regions},{"nodes",nodes}};
    const auto response_bytes=result.dump().size();
    if(response_bytes>network_response_max_bytes) throw std::length_error("network response bytes actual="+std::to_string(response_bytes)+" required<="+std::to_string(network_response_max_bytes));
    return result;
}
#ifdef MT2MP_NETWORK_TEST
namespace testing {
void backend(uintptr_t base,Getter map_capacity,Getter region_capacity,Getter extra_supply) {
    test_base=base; test_map=map_capacity; test_region=region_capacity; test_extra=extra_supply;
}
}
#endif
}
