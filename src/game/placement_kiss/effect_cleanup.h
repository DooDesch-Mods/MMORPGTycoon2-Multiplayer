#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace placement_kiss::effect_cleanup {
struct Receipt {
    bool checked{}, present{}, attempted{}, returned{}, removed{}, slot_cleared{};
    std::int32_t count_before{-1}, count_after{-1};
    std::string cause;
};

// The manager owns the instance; the private Build cursor owns its effect.
// Destroy the instance before Build D0 destroys that effect at cursor+0x6D8.
template<class Port>
bool release(Port& port, std::uintptr_t cursor, std::uintptr_t expected_shard, Receipt& result) {
    if(result.checked) { result.cause="preview effect cleanup already inspected; cannot replay"; return false; }
    result.checked=true;
    const auto fail=[&](const std::string& cause) { result.cause=cause; return false; };
    std::uintptr_t instance=0;
    if(!port.read(cursor+0x6e0,&instance,sizeof(instance))) return fail("preview effect slot unreadable");
    if(!instance) { result.slot_cleared=true; return true; }
    result.present=true;
    std::uintptr_t shard=0,owner=0,manager=0;
    if(!port.read(instance+0x10,&shard,sizeof(shard))) return fail("preview effect shard unreadable");
    if(!expected_shard||shard!=expected_shard) return fail("preview effect shard differs from owned shard");
    if(!port.read(instance+8,&owner,sizeof(owner))) return fail("preview effect manager owner unreadable");
    if(!port.manager(shard,manager)||!manager) return fail("preview effect original GetEffectManager failed");
    if(owner!=manager) return fail("preview effect manager differs from original owner");
    const auto snapshot=[&](std::vector<std::uintptr_t>& values,std::int32_t& count) {
        std::uintptr_t data=0;std::int32_t capacity=0;
        if(!port.read(manager+0x68,&data,sizeof(data))||!port.read(manager+0x70,&count,sizeof(count))||!port.read(manager+0x74,&capacity,sizeof(capacity))) return fail("preview effect manager header unreadable");
        if(count<0||capacity<count||count>1048576||(!data&&count)) return fail("preview effect manager count actual="+std::to_string(count)+" capacity="+std::to_string(capacity)+" required=0..min(capacity,1048576) with data");
        values.resize(static_cast<std::size_t>(count));
        if(count&&!port.read(data,values.data(),values.size()*sizeof(std::uintptr_t))) return fail("preview effect manager members unreadable");
        return true;
    };
    std::vector<std::uintptr_t> before,after;
    if(!snapshot(before,result.count_before)) return false;
    const auto matches=std::count(before.begin(),before.end(),instance);
    if(matches!=1) return fail("preview effect manager membership actual="+std::to_string(matches)+" required=1");
    const auto index=static_cast<std::size_t>(std::find(before.begin(),before.end(),instance)-before.begin());
    before[index]=before.back();before.pop_back();
    result.attempted=true;
    if(!port.destroy(manager,instance)) return fail("preview effect original DestroyInstance failed; cannot replay");
    result.returned=true;
    // The original call freed instance. Only the manager and cursor are read below.
    if(!snapshot(after,result.count_after)) return false;
    if(after!=before) return fail("preview effect manager removal differs: count actual="+std::to_string(result.count_after)+" required="+std::to_string(result.count_before-1)+" and exact original swap removal");
    result.removed=true;
    if(!port.clear(cursor+0x6e0,instance)) return fail("preview effect cursor slot clear failed; cannot replay");
    std::uintptr_t remaining=instance;
    if(!port.read(cursor+0x6e0,&remaining,sizeof(remaining))||remaining) return fail("preview effect cursor slot did not read as zero");
    result.slot_cleared=true;
    return true;
}
}
