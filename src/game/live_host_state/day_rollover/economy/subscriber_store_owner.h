#pragma once
#include "monetary.h"
namespace mt2mp::game::live_host_state::day::economy {
// The Apply transaction retains complete before/final reads. Each store checks
// its live receiver directly instead of resampling every unrelated Subscriber.
// Scalar CAS has no engine callbacks. Check live roots on every store while the
// complete History receipt is retained only within that synchronous phase.
template<class Read>bool scalar_store_roots(Read read,const Pin& p,std::string& error){
    auto fail=[&](const char* cause){if(error.empty())error=cause;return false;};
    uintptr_t clock=0,business=0,record=0,history=0,type=0,member=0;
    int32_t selected=-1,count=-1,capacity=-1,version=-1,cash=0;
    if(!read(p.image,0x13df9a0,clock,error)||clock!=p.clock||!read(clock,0,type,error)||type!=p.image+0x17499d0||!read(clock,0x58,selected,error)||selected!=0||
       !read(p.image,0x13df220,business,error)||business!=p.business||!read(business,0,type,error)||type!=p.image+0x16d97e0||!read(business,0x100,record,error)||record!=p.record||
       !read(record,0,type,error)||type!=p.image+0x1712c00||!read(record,0x18,version,error)||version!=2||!read(business+0x60,0,type,error)||type!=p.image+0x16ec280||
       !read(business,0x68,history,error)||history!=p.history||!read(business,0x70,count,error)||count!=p.history_count||!read(business,0x74,capacity,error)||capacity<count||
       !read(business,0x38,cash,error)||cash!=p.values.cash)return fail("economic scalar live Clock/Business/History/CurrentRecord roots differ");
    size_t matches=0;
    for(int32_t i=0;i<count;++i){if(!read(history,size_t(i)*8,member,error))return false;if(member==record)++matches;}
    if(matches!=1)return fail("economic scalar CurrentRecord is not a unique live History member");
    error.clear();return true;
}
template<class Read>bool subscriber_store_owner(Read read,const Owners& before,const NativeHistory& actual,size_t index,std::string& error){
    auto fail=[&](const char* cause){if(error.empty())error=cause;return false;};
    const auto& p=actual.pin;const auto& old=before.history.pin;
    if(before.subscribers.size()!=before.state.subscribers.size()||before.subscribers.size()!=before.needs_owners.size()||before.subscribers.size()!=before.subscriber_slots.size()||index>=before.subscribers.size())return fail("economic direct Subscriber store index differs");
    if(p.context!=old.context||p.binding!=old.binding||p.world!=old.world||p.clock!=old.clock||p.business!=old.business||p.record!=old.record||p.history!=old.history||p.history_count!=old.history_count||actual.owners!=before.history.owners||actual.records!=before.history.records||p.values!=old.values||p.common_epoch!=old.common_epoch||p.selected!=0||p.context.role!=action_session::Role::Client)return fail("economic direct Subscriber current Day/History/finance/Clock owner differs");
    struct Arrays {uintptr_t manager=0,subscribers=0,loans=0,offers=0;int32_t next_id=-1;std::array<int32_t,6> sizes{};bool operator==(const Arrays&)const=default;};
    auto arrays=[&](Arrays& out){
        uintptr_t type=0;
        if(!read(p.image,0x13df690,out.manager,error)||out.manager!=before.subscriber_manager||!read(out.manager,0,type,error)||type!=p.image+0x1725f40||!read(out.manager,0x40,out.subscribers,error)||!read(out.manager,0x48,out.sizes[0],error)||!read(out.manager,0x4c,out.sizes[1],error)||!read(out.manager,0x68,out.next_id,error)||
           !read(p.business,0x98,out.loans,error)||!read(p.business,0xa0,out.sizes[2],error)||!read(p.business,0xa4,out.sizes[3],error)||!read(p.business,0xc8,out.offers,error)||!read(p.business,0xd0,out.sizes[4],error)||!read(p.business,0xd4,out.sizes[5],error))return fail("economic direct Subscriber Manager/Loan/Offer header unreadable");
        if(out.subscribers!=before.subscriber_data||out.loans!=before.loan_data||out.offers!=before.offer_data||out.sizes[0]<0||out.sizes[0]>1000000||out.sizes[1]<out.sizes[0]||size_t(out.sizes[0])!=before.subscribers.size()||out.sizes[2]<0||out.sizes[2]>1024||out.sizes[3]<out.sizes[2]||size_t(out.sizes[2])!=before.loans.size()||out.sizes[4]<0||out.sizes[4]>1024||out.sizes[5]<out.sizes[4]||size_t(out.sizes[4])!=before.offers.size()||out.next_id<0)return fail("economic direct Subscriber Manager/Loan/Offer array identity differs");
        return true;
    };
    Arrays first,last;if(!arrays(first))return false;
    uintptr_t receiver=0;const auto& row=before.state.subscribers[index];const size_t slot=before.subscriber_slots[index];
    if(slot>=size_t(first.sizes[0]))return fail("economic direct Subscriber native slot outside current array");
    if(first.subscribers>UINTPTR_MAX-slot*sizeof(uintptr_t)||!read(first.subscribers,slot*sizeof(uintptr_t),receiver,error)||receiver!=before.subscribers[index]||row.serialized_id<0||row.serialized_id>=first.next_id)return fail("economic direct Subscriber actual array slot/ID differs");
    subscriber_needs::Owner owner;subscriber_needs::Bits needs;
    subscriber_activity::State activity;
    if(!subscriber_needs::sample(read,p.image,receiver,row.serialized_id,owner,needs,error)||owner!=before.needs_owners[index]||!subscriber_activity::sample(read,p.image,receiver,row.serialized_id,activity,error))return fail("economic direct Subscriber receiver/Brain/NeedSet owner differs");
    uintptr_t final_receiver=0;
    if(!arrays(last)||last!=first||!read(last.subscribers,slot*sizeof(uintptr_t),final_receiver,error)||final_receiver!=receiver)return fail("economic direct Subscriber owner changed during store admission");
    error.clear();return true;
}
}
