#pragma once
#include "gm_policy_state.h"
#include "gm_ticket_state.h"
#include <algorithm>
#include <set>
namespace mt2mp::game::gm_policy {
struct Cascade {
    gm_ticket::State gm;
    std::array<std::vector<int32_t>,2> managers;
    std::vector<bool> models;
    bool operator==(const Cascade&)const=default;
};
inline bool valid(const Cascade& value,std::string& error){
    if(!gm_ticket::valid(value.gm,error))return false;
    if(value.models.size()!=value.gm.masters.size()){error="GM policy model witness extent differs from fixed pool";return false;}
    std::set<int32_t> all;
    for(const auto& list:value.managers)for(auto slot:list)if(slot<0||size_t(slot)>=value.gm.masters.size()||!all.insert(slot).second){error="GM policy manager membership is not unique fixed slots";return false;}
    for(size_t i=0;i<value.models.size();++i)if(value.models[i]!=value.gm.masters[i].on_map){error="GM policy model membership differs from OnMap";return false;}
    return true;
}
inline nlohmann::json encode(const Cascade& value){return {{"gm",gm_ticket::encode(value.gm)},{"managers",value.managers},{"models",value.models}};}
inline bool expected(const Intent& intent,const State& policies,const Cascade& before,Cascade& after,std::string& error){
    if(!valid(intent)||!valid(policies)||!valid(before,error)||policies.policies[intent.policy][size_t(intent.field)]!=intent.before){if(error.empty())error="GM policy complete before cascade differs";return false;}
    auto desired=policies;desired.policies[intent.policy][size_t(intent.field)]=intent.value;after=before;
    after.gm.tickets.clear();std::vector<int32_t> remap(before.gm.tickets.size(),-1);
    for(size_t index=0;index<before.gm.tickets.size();++index){const auto& ticket=before.gm.tickets[index];
        const bool remove=(ticket.type==2&&!desired.policies[0][0])||(ticket.type==3&&!desired.policies[1][0]);
        if(!remove){remap[index]=int32_t(after.gm.tickets.size());after.gm.tickets.push_back(ticket);continue;}
        if(ticket.master<0)continue;const auto slot=ticket.master;auto& master=after.gm.masters[size_t(slot)];
        master.ticket=-1;master.state=0;master.on_map=false;after.models[size_t(slot)]=false;
        auto& active=after.managers[1];const auto found=std::find(active.begin(),active.end(),slot);
        if(found!=active.end())active.erase(found);
        auto& idle=after.managers[0];if(std::find(idle.begin(),idle.end(),slot)!=idle.end()){error="GM policy original Lounge Enter would duplicate an idle master";return false;}
        idle.push_back(slot);
    }
    for(auto& master:after.gm.masters)if(master.ticket>=0)master.ticket=remap[size_t(master.ticket)];
    return valid(after,error);
}
struct PolicyArray {uintptr_t data=0;int32_t count=0,capacity=0;std::vector<uintptr_t> members;bool operator==(const PolicyArray&)const=default;};
template<class Read>bool policy_array(Read& read,uintptr_t owner,size_t offset,size_t limit,PolicyArray& out,std::string& error){
    out={};if(!read(owner,offset,out.data,error)||!read(owner,offset+8,out.count,error)||!read(owner,offset+12,out.capacity,error))return false;
    if(out.count<0||out.capacity<out.count||size_t(out.count)>limit||size_t(out.capacity)>std::max<size_t>(4096,limit)||(!out.data&&out.capacity)){error="GM policy native membership array extent invalid";return false;}
    std::set<uintptr_t> seen;
    for(int32_t index=0;index<out.count;++index){uintptr_t member=0;if(!read(out.data,size_t(index)*8,member,error))return false;if(!member||!seen.insert(member).second){error="GM policy native membership array contains null/duplicate";return false;}out.members.push_back(member);}
    uintptr_t final_data=0;int32_t final_count=0,final_capacity=0;
    if(!read(owner,offset,final_data,error)||!read(owner,offset+8,final_count,error)||!read(owner,offset+12,final_capacity,error))return false;
    if(final_data!=out.data||final_count!=out.count||final_capacity!=out.capacity){error="GM policy native membership array changed during read";return false;}
    return true;
}
template<class Read>bool managers(Read& read,uintptr_t lounge,const std::vector<uintptr_t>& masters,std::array<uintptr_t,2>& owners,std::array<std::vector<int32_t>,2>& out,std::string& error){
    out={};owners={};std::set<int32_t> unique;
    for(size_t index=0;index<2;++index){if(!read(lounge,0x68+index*8,owners[index],error)||!owners[index]){if(error.empty())error="GM policy Lounge manager owner absent";return false;}
        PolicyArray values;if(!policy_array(read,owners[index],0x38,50,values,error))return false;
        for(auto member:values.members){const auto found=std::find(masters.begin(),masters.end(),member);if(found==masters.end()){error="GM policy manager member absent from saved fixed pool";return false;}
            const auto slot=int32_t(found-masters.begin());if(!unique.insert(slot).second){error="GM policy manager member occurs in both managers";return false;}out[index].push_back(slot);}
    }
    if(owners[0]==owners[1]){error="GM policy Lounge managers alias";return false;}
    return true;
}
}
