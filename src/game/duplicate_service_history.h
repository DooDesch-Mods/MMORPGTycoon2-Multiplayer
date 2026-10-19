#pragma once
#include "npc_quest_targets.h"
#include "service_membership_reader.h"
namespace mt2mp::game::duplicate_service_history {
namespace q=npc_quest_targets;
struct Daily {
    std::vector<int32_t> entries,history;
    std::array<int32_t,6> words{};
    uint8_t warning=0;
    bool operator==(const Daily&)const=default;
};
struct Saved {
    uintptr_t object=0,owner=0,destination=0;
    uint64_t uid=0,owner_uid=0,destination_uid=0;
    int32_t type=0;
    std::array<int32_t,2> price{};
    std::array<uint8_t,3> flags{};
    std::array<Daily,2> counts;
    bool operator==(const Saved&)const=default;
};
using World=std::map<uint64_t,std::vector<Saved>>;
struct Removal {
    std::map<uint64_t,Saved> saved;
    std::map<uint64_t,std::vector<uint64_t>> remaining;
    World survivors;
    std::set<uint64_t> related_owners;
};
// GetPrice copies the resolved config Price into the live Service. Derive the
// same value from the complete current config witness during preflight so the
// original getter does not mutate a pending request before graph execution.
inline bool effective_price(const action_session::ServiceMembershipState& state,const service_membership_reader::Local& local,
                            size_t index,std::array<int32_t,2>& out,std::string& e){
    if(index>=state.rows.size()||state.rows.size()!=local.rows.size())return q::fail(e,"Duplicate Service price row count differs");
    const auto& row=state.rows[index];const auto& pin=local.rows[index];out=row.price;
    if(!row.flags[0])return true;
    if(pin.config){const auto found=std::find(local.configs.begin(),local.configs.end(),pin.config);
        if(found==local.configs.end()||size_t(found-local.configs.begin())>=state.configs.size()||
           state.configs[size_t(found-local.configs.begin())].key!=row.config_key)
            return q::fail(e,"Duplicate Service effective Price cached config owner/key differs");
        out=state.configs[size_t(found-local.configs.begin())].price;return true;
    }
    if(!state.config_present)return true;
    if(row.config_key.empty())return q::fail(e,"Duplicate Service effective Price original config name is empty");
    const auto found=std::find_if(state.configs.begin(),state.configs.end(),[&](const auto& config){return config.key==row.config_key;});
    if(found!=state.configs.end())out=found->price;
    return true;
}
inline bool removal(const World& before,const std::vector<uint64_t>& ordered_buildings,Removal& out,std::string& e){
    out={};auto current=before;std::set<uint64_t> identities,targets;
    for(const auto& [owner,rows]:current)for(const auto& row:rows)
        if(!owner||row.owner_uid!=owner||!row.uid||!identities.insert(row.uid).second)
            return q::fail(e,"Duplicate Service complete owner/full UID membership differs");
    auto save=[&](const Saved& row){
        if(!out.saved.emplace(row.uid,row).second)return q::fail(e,"Duplicate Service native Destroy saves one UID twice");
        return true;};
    for(auto uid:ordered_buildings){const auto found=current.find(uid);
        if(!targets.insert(uid).second||found==current.end())return q::fail(e,"Duplicate Service Destroy Building owner absent or repeated");
        const auto rows=found->second;
        for(const auto& row:rows){if(!save(row))return false;if(!row.destination_uid)continue;
            const auto destination=current.find(row.destination_uid);
            if(destination==current.end())return q::fail(e,"Duplicate Service Destroy destination outside complete current Building set");
            auto& peers=destination->second;
            const auto peer=std::find_if(peers.begin(),peers.end(),[&](const auto& candidate){return candidate.type==row.type&&candidate.destination_uid==uid;});
            // Building::Destroy removes only the first reciprocal Service of
            // the same type. It does not remove unrelated services there.
            if(peer!=peers.end()){
                if(peer->uid==row.uid)return q::fail(e,"Duplicate Service self-reciprocal Destroy is not a distinct native member");
                if(!save(*peer))return false;out.related_owners.insert(destination->first);peers.erase(peer);
            }
        }
        current.erase(uid);
    }
    for(const auto& [owner,rows]:current){auto& values=out.remaining[owner];for(const auto& row:rows)values.push_back(row.uid);}
    out.survivors=std::move(current);
    return true;
}
inline bool absent(const Removal& expected,const World& after,std::string& e){
    if(expected.remaining.size()!=after.size())return q::fail(e,"Duplicate Service complete surviving Building count differs");
    for(const auto& [owner,rows]:after){const auto found=expected.remaining.find(owner);
        if(found==expected.remaining.end()||found->second.size()!=rows.size())return q::fail(e,"Duplicate Service survivor owner/count differs");
        for(size_t i=0;i<rows.size();++i)if(rows[i].owner_uid!=owner||rows[i].uid!=found->second[i]||expected.saved.contains(rows[i].uid)||rows[i]!=expected.survivors.at(owner)[i])
            return q::fail(e,"Duplicate Service survivor full UID/order differs");}
    return true;
}
template<class Memory>
bool daily(Memory& m,uintptr_t image,uintptr_t pointer,Daily& out,std::string& e){
    out={};
    if(!q::typed(m,image,pointer,0x16e4710,e))return false;
    // The original copy constructor copies both arrays, then the six
    // DailyCount scalar words and warning byte. Capacities may change.
    for(size_t i=0;i<2;++i){const auto offset=i?size_t(0x40):size_t(0x28);uintptr_t data=0;int32_t count=0,capacity=0;
        if(!q::at(m,pointer,offset,data,e)||!q::at(m,pointer,offset+8,count,e)||!q::at(m,pointer,offset+12,capacity,e))return false;
        if(count<0||count>100000||capacity<count||capacity>1000000||(!data&&capacity)||data>UINTPTR_MAX-size_t(capacity)*4)
            return q::fail(e,"Duplicate Service DailyCount array bounds differ");
        auto& values=i?out.history:out.entries;values.resize(size_t(count));
        if(count&&!m.read(data,values.data(),values.size()*sizeof(int32_t),e))return false;
        uintptr_t current=0;int32_t current_count=0,current_capacity=0;
        if(!q::at(m,pointer,offset,current,e)||!q::at(m,pointer,offset+8,current_count,e)||!q::at(m,pointer,offset+12,current_capacity,e))return false;
        if(current!=data||current_count!=count||current_capacity!=capacity)return q::fail(e,"Duplicate Service DailyCount array changed during read");
    }
    if(!q::at(m,pointer,0x50,out.words,e)||!q::at(m,pointer,0x68,out.warning,e))return false;
    return out.warning<=1||q::fail(e,"Duplicate Service DailyCount warning invalid");
}
template<class Memory,class Find>
bool capture(Memory& m,uintptr_t image,uintptr_t object,uintptr_t owner,const std::array<int32_t,2>& effective_price,
             Find&& find,Saved& out,std::string& e){
    out={};out.object=object;uintptr_t actual=0;
    if(!q::typed(m,image,object,0x1718480,e)||!q::at(m,object,0x18,out.uid,e)||!out.uid||
       !q::at(m,object,0x148,out.owner,e)||out.owner!=owner||!q::at(m,owner,0x18,out.owner_uid,e)||!out.owner_uid||
       !find(out.uid,actual,e)||actual!=object||!q::at(m,object,0x20,out.type,e)||
       !q::at(m,object,0x48,out.flags,e)||!q::at(m,object,0x130,out.destination,e))
        return q::fail(e,e.empty()?"Duplicate Service current class/full UID/owner differs":e);
    if(out.type<0||out.type>16||std::any_of(out.flags.begin(),out.flags.end(),[](auto v){return v>1;}))
        return q::fail(e,"Duplicate Service current type/flags invalid");
    if(!find(out.owner_uid,actual,e)||actual!=owner)return q::fail(e,e.empty()?"Duplicate Service current owner UID differs":e);
    if(out.destination&&(!q::at(m,out.destination,0x18,out.destination_uid,e)||!out.destination_uid||
       !find(out.destination_uid,actual,e)||actual!=out.destination))return q::fail(e,e.empty()?"Duplicate Service current destination UID differs":e);
    out.price=effective_price;
    return daily(m,image,object+0x50,out.counts[0],e)&&daily(m,image,object+0xc0,out.counts[1],e);
}
template<class Memory,class Find>
bool inverse(Memory& m,uintptr_t image,uintptr_t node,const Saved& saved,Find&& find,std::string& e){
    uintptr_t method=0,actual=0;uint64_t uid=0,owner=0,destination=0;int32_t type=-1;
    std::array<int32_t,2> price{};std::array<uint8_t,3> flags{};std::array<Daily,2> counts;
    if(!q::typed(m,image,node,0x1737830,e)||!q::at(m,image+0x1737830,0x10,method,e)||method!=image+0x547370||
       !q::at(m,node,0xb8,uid,e)||!q::at(m,node,0xc0,type,e)||!q::typed(m,image,node+0xc8,0x174ab50,e)||
       !q::at(m,node,0xe0,price,e)||!q::at(m,node,0x108,flags,e)||!q::at(m,node,0x1f0,owner,e)||
       !q::at(m,node,0x1f8,destination,e)||!daily(m,image,node+0x110,counts[0],e)||!daily(m,image,node+0x180,counts[1],e))return false;
    if(uid!=saved.uid||owner!=saved.owner_uid||destination!=saved.destination_uid||type!=saved.type||price!=saved.price||flags!=saved.flags||counts!=saved.counts)
        return q::fail(e,"Duplicate Service inverse UID/owner/destination/type/price/flags/full DailyCount differs");
    if(!find(uid,actual,e)||actual)return q::fail(e,e.empty()?"Duplicate Service inverse target UID is still alive":e);
    return true;
}
template<class Memory,class Find>
bool restored(Memory& m,uintptr_t image,uintptr_t object,uintptr_t owner,const Saved& saved,Find&& find,std::string& e){
    Saved current;if(!capture(m,image,object,owner,saved.price,find,current,e))return false;
    std::array<int32_t,2> actual_price{};
    if(!q::typed(m,image,object+0x28,0x174ab50,e)||!q::at(m,object,0x40,actual_price,e))return false;
    return (current.uid==saved.uid&&current.owner_uid==saved.owner_uid&&current.destination_uid==saved.destination_uid&&
            current.type==saved.type&&actual_price==saved.price&&current.flags==saved.flags&&current.counts==saved.counts)||
        q::fail(e,"Duplicate Service restored original fields/full DailyCount differ");
}
}
