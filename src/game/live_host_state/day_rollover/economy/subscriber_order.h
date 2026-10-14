#pragma once
#include "monetary.h"
#include <algorithm>
#include <numeric>
namespace mt2mp::game::live_host_state::day::economy {
inline bool valid_subscriber_slots(const Owners& owner,std::string& error){
    const size_t count=owner.state.subscribers.size();
    if(owner.subscribers.size()!=count||owner.needs_owners.size()!=count||owner.subscriber_slots.size()!=count){error="economic Subscriber aligned local owner/slot shape differs";return false;}
    std::vector<unsigned char> slots(count,0);
    for(size_t slot:owner.subscriber_slots){if(slot>=count||slots[slot]){error="economic Subscriber native slot mapping is not a unique current permutation";return false;}slots[slot]=1;}
    error.clear();return true;
}
inline bool canonical_subscribers(Owners& owner,std::string& error){
    if(!valid_subscriber_slots(owner,error))return false;
    const size_t count=owner.state.subscribers.size();
    std::vector<size_t> order(count);std::iota(order.begin(),order.end(),size_t(0));
    std::sort(order.begin(),order.end(),[&](size_t a,size_t b){return owner.state.subscribers[a].serialized_id<owner.state.subscribers[b].serialized_id;});
    int32_t previous=-1;
    for(size_t index:order){const int32_t id=owner.state.subscribers[index].serialized_id;if(id<=previous||id>=owner.next_id){error="economic canonical Subscriber ID duplicate/negative/outside nextID";return false;}previous=id;}
    auto rows=std::move(owner.state.subscribers);auto receivers=std::move(owner.subscribers);
    auto needs=std::move(owner.needs_owners);auto slots=std::move(owner.subscriber_slots);
    owner.state.subscribers.clear();owner.subscribers.clear();owner.needs_owners.clear();owner.subscriber_slots.clear();
    owner.state.subscribers.reserve(count);owner.subscribers.reserve(count);owner.needs_owners.reserve(count);owner.subscriber_slots.reserve(count);
    for(size_t index:order){owner.state.subscribers.push_back(std::move(rows[index]));owner.subscribers.push_back(receivers[index]);owner.needs_owners.push_back(needs[index]);owner.subscriber_slots.push_back(slots[index]);}
    error.clear();return true;
}
}
