#pragma once
#include "npc_quest_runtime_state.h"
#include "npc_quest_records.h"

namespace mt2mp::npc_quest_runtime {
template<class Memory>bool read(Memory& memory,uintptr_t image,uintptr_t quest,Value& out,std::string& error){
    out={};uintptr_t ap=0;
    if(!quest||quest>UINTPTR_MAX-0x220){error="Quest runtime address null or overflows";return false;}
    if(!memory.copy(quest,&ap,sizeof ap,error))return false;
    if(ap!=image+0x174ac10){error="Quest runtime class addresspoint differs";return false;}
    npc_quest_records::Rating rating;
    if(!npc_quest_records::read_rating(memory,image,quest,rating,error)||
       !npc_quest_records::get(memory,quest,0x88,out.xp_reward,"runtime XP reward",error)||
       !npc_quest_records::get(memory,quest,0x8c,out.gold_reward,"runtime gold reward",error))return false;
    out.rating_bits={std::bit_cast<uint32_t>(rating.beauty),std::bit_cast<uint32_t>(rating.convenience),std::bit_cast<uint32_t>(rating.danger)};
    out.rating_set=rating.set;return valid(out,error);
}
struct ApplyReceipt {
    bool attempted=false,confirmed=false,unknown=false,advertisement_attempted=false,advertisement_returned=false;
    uint32_t stores=0;
    Value before,required,after;
    std::string cause;
};
template<class Calls>ApplyReceipt apply(Calls& calls,uintptr_t quest,const Value& old,const Value& target){
    ApplyReceipt r;r.before=old;r.required=target;
    const auto failed=[&](const char* fallback){if(r.cause.empty())r.cause=fallback;r.unknown=r.attempted;return r;};
    if(!valid(old,r.cause)||!valid(target,r.cause)||!calls.guard(r.cause)||!calls.read(r.after,r.cause))return failed("Quest runtime apply admission failed");
    if(r.after!=old){r.cause="Quest runtime complete prior value changed";return failed("");}
    if(old==target){r.confirmed=true;return r;}
    const auto scalar=[&](size_t offset,uint32_t a,uint32_t b){
        if(a==b)return true;
        if(!calls.guard(r.cause))return false;
        r.attempted=true;
        if(!calls.compare32(quest+offset,a,b,r.cause)){
            r.cause="Quest runtime CAS offset="+std::to_string(offset)+" expected_bits="+std::to_string(a)+
                " required_bits="+std::to_string(b)+" original="+r.cause;return false;
        }
        ++r.stores;return true;
    };
    for(size_t i=0;i<3;++i)if(!scalar(0x1f8+i*4,old.rating_bits[i],target.rating_bits[i]))return failed("Quest rating scalar CAS failed");
    if(!scalar(0x88,std::bit_cast<uint32_t>(old.xp_reward),std::bit_cast<uint32_t>(target.xp_reward))||
       !scalar(0x8c,std::bit_cast<uint32_t>(old.gold_reward),std::bit_cast<uint32_t>(target.gold_reward)))return failed("Quest reward scalar CAS failed");
    if(old.rating_set!=target.rating_set){
        if(!calls.guard(r.cause))return failed("Quest runtime flag guard failed");
        r.attempted=true;
        if(!calls.compare8(quest+0x204,uint8_t(old.rating_set),uint8_t(target.rating_set),r.cause))return failed("Quest runtime flag CAS failed");
        ++r.stores;
    }
    if(!calls.guard(r.cause))return failed("Quest runtime advertisement guard failed");
    r.attempted=true;r.advertisement_attempted=true;
    if(!calls.advertise(r.cause))return failed("Quest original advertisement refresh failed");
    r.advertisement_returned=true;
    if(!calls.guard(r.cause)||!calls.read(r.after,r.cause))return failed("Quest runtime complete readback failed");
    if(r.after!=target){r.cause="Quest runtime complete target readback differs";return failed("");}
    r.confirmed=true;return r;
}
struct CompletionReceipt {
    bool owned=false,client=false,attempted=false,returned=false,suppressed=false,confirmed=false;
    Value before,after;
    std::string cause;
};
template<class Calls>CompletionReceipt complete(Calls& calls,uintptr_t quest,uintptr_t path,uintptr_t caller,uintptr_t image){
    CompletionReceipt r;
    if(!calls.active(r.client)){
        r.attempted=true;r.returned=calls.original(quest,path,r.cause);r.confirmed=r.returned;return r;
    }
    if(!calls.owner(quest,r.owned,r.cause))return r;
    if(!r.owned){r.attempted=true;r.returned=calls.original(quest,path,r.cause);r.confirmed=r.returned;return r;}
    if(caller!=image+0x4d8f13&&caller!=image+0x4d92a4){r.cause="Quest rating owned physical Tick caller differs";return r;}
    if(!calls.read(quest,r.before,r.cause))return r;
    r.attempted=true;
    if(r.client){r.suppressed=true;r.returned=calls.advertise(quest,r.cause);}
    else r.returned=calls.original(quest,path,r.cause);
    if(!r.returned||!calls.owner(quest,r.owned,r.cause)||!r.owned||!calls.read(quest,r.after,r.cause)){
        if(r.cause.empty())r.cause="Quest rating completion owner/readback changed";return r;
    }
    if(r.client&&r.after!=r.before){r.cause="Quest Client completion changed authoritative rating/reward";return r;}
    r.confirmed=true;return r;
}
}
