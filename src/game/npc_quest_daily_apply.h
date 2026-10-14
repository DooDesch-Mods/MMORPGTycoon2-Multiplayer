#pragma once

#include "npc_quest_state.h"
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace mt2mp::npc_quest_daily_apply {

using npc_quest_records::DailyCount;

struct Receipt {
    bool reserve_attempted=false;
    bool reserve_confirmed=false;
    bool mutation_attempted=false;
    bool unknown=false;
    bool changed=false;
    bool confirmed=false;
};

struct NativeState {
    uintptr_t data=0;
    // Count, capacity, today, total, day, retention, highest, local revision.
    std::array<int32_t,8> words{};
    uint8_t warning=0;
    DailyCount value;
};

inline bool reject(std::string& error,const char* cause){
    error=cause;return false;
}

template<class Memory>
bool read_native(Memory& memory,uintptr_t image,uintptr_t daily,
                 NativeState& out,std::string& error){
    out={};uintptr_t ap=0,array_ap=0;
    if(!daily||daily>UINTPTR_MAX-0x69||image>UINTPTR_MAX-0x1749370)
        return reject(error,"DailyCount address invalid");
    if(!memory.copy(daily,&ap,sizeof(ap),error)||
       !memory.copy(daily+0x38,&array_ap,sizeof(array_ap),error)||
       !memory.copy(daily+0x40,&out.data,sizeof(out.data),error)||
       !memory.copy(daily+0x48,out.words.data(),sizeof(out.words),error)||
       !memory.copy(daily+0x68,&out.warning,sizeof(out.warning),error))return false;
    if(ap!=image+0x16e4710||array_ap!=image+0x1749370)
        return reject(error,"DailyCount or history AP differs");
    const int32_t count=out.words[0],capacity=out.words[1];
    if(out.words[5]!=10||count<0||count>10||capacity<count||capacity>100000||
       (capacity&&!out.data)||out.data>UINTPTR_MAX-size_t(capacity)*sizeof(int32_t)||
       out.warning>1)
        return reject(error,"DailyCount history span, retention or warning differs");
    out.value.history.resize(size_t(count));
    out.value.today=out.words[2];out.value.total=out.words[3];
    return !count||memory.copy(out.data,out.value.history.data(),
                              size_t(count)*sizeof(int32_t),error);
}

// The caller holds the exclusive client mainthread/SaveScope and current FrameBinding.
// compare_store(address, expected, desired, byte_count, error) must check before writing.
// reserve(array, capacity, error) calls original E8080 once; false may have mutated.
// guard(error) revalidates world, owner and frame before each mutation.
// A false result after mutation_attempted requires the owner's Unknown/world block.
template<class Memory,class CompareStore,class Reserve,class Guard>
bool apply_daily(Memory& memory,uintptr_t image,uintptr_t object,size_t offset,
                 const DailyCount& before,const DailyCount& target,
                 int32_t highest_cache,int32_t current_day,
                 CompareStore&& compare_store,Reserve&& reserve,Guard&& guard,
                 Receipt& receipt,std::string& error){
    receipt={};
    const auto failed=[&](){receipt.unknown=receipt.mutation_attempted;return false;};
    if(offset>UINTPTR_MAX-object||target.history.size()>10)
        return reject(error,"DailyCount target address or history length invalid");
    const uintptr_t daily=object+offset;
    NativeState state;
    if(!guard(error)||!read_native(memory,image,daily,state,error))return false;
    if(state.value!=before)return reject(error,"DailyCount preflight value differs");
    if(state.value==target&&state.words[4]==current_day&&state.words[6]==highest_cache){
        receipt.confirmed=true;return true;
    }
    if(size_t(state.words[1])<target.history.size()){
        if(!guard(error))return false;
        receipt.reserve_attempted=true;receipt.mutation_attempted=true;
        if(!reserve(daily+0x38,10,error))return failed();
        NativeState grown;
        if(!read_native(memory,image,daily,grown,error))return failed();
        auto expected=state.words;expected[1]=10;
        if(grown.words!=expected||grown.value!=state.value||grown.warning!=state.warning){
            reject(error,"DailyCount original Reserve readback differs");return failed();
        }
        state=std::move(grown);receipt.reserve_confirmed=true;
    }
    std::vector<int32_t> old_slots(target.history.size());
    if(!old_slots.empty()&&!memory.copy(state.data,old_slots.data(),
                                       old_slots.size()*sizeof(int32_t),error))return failed();
    if(old_slots!=target.history){
        if(!guard(error))return failed();
        receipt.mutation_attempted=true;
        if(!compare_store(state.data,old_slots.data(),target.history.data(),
                          old_slots.size()*sizeof(int32_t),error))return failed();
    }
    auto desired=state.words;
    desired[0]=static_cast<int32_t>(target.history.size());
    desired[2]=target.today;desired[3]=target.total;
    desired[4]=current_day;desired[6]=highest_cache;
    desired[7]=std::bit_cast<int32_t>(std::bit_cast<uint32_t>(state.words[7])+uint32_t{1});
    if(!guard(error))return failed();
    receipt.mutation_attempted=true;
    if(!compare_store(daily+0x48,state.words.data(),desired.data(),sizeof(desired),error))
        return failed();
    NativeState after;
    if(!guard(error)||!read_native(memory,image,daily,after,error))return failed();
    if(after.data!=state.data||after.words!=desired||after.warning!=state.warning||
       after.value!=target){
        reject(error,"DailyCount final readback differs");return failed();
    }
    receipt.changed=true;receipt.confirmed=true;return true;
}

} // namespace mt2mp::npc_quest_daily_apply
