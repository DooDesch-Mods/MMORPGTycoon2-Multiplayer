#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace mt2mp::game::gm_ticket {
struct Span {
    uintptr_t object=0,data=0;int32_t count=0,capacity=0;
    std::vector<uintptr_t> members;bool operator==(const Span&)const=default;
};
struct Ticket {
    uintptr_t object=0,toon=0,toon_proxy=0,region=0,master=0;
    int32_t type=0;std::array<float,3> position{};uint8_t valid=0;bool toon_alive=false;
    bool operator==(const Ticket&)const=default;
};
struct Master {
    uintptr_t object=0,ticket=0,region=0,root=0,arrow=0;uint64_t uid=0;
    int32_t slot=-1,state=0;std::array<float,3> position{},timers{},scale{},arrow_target{};std::array<float,4> orientation{};
    uint8_t employed=0,active=0,on_map=0,evidence=0,arrow_has_target=0;
    bool operator==(const Master&)const=default;
};
struct Census {
    uintptr_t queue=0;Span tickets;std::vector<Ticket> values;std::vector<Master> masters;
    bool operator==(const Census&)const=default;
};
inline bool fail(std::string& error,const char* cause){error=cause;return false;}

template<class Read>bool span(Read& read,uintptr_t image,uintptr_t object,Span& out,std::string& error){
    out={};out.object=object;uintptr_t ap=0;
    if(!read(object,0,ap,error)||!read(object,8,out.data,error)||!read(object,0x10,out.count,error)||!read(object,0x14,out.capacity,error))return false;
    if(ap!=image+0x16f0720||out.count<0||out.capacity<out.count||out.count>4096||(!out.data&&out.capacity)||
       (out.count&&size_t(out.count)>(UINTPTR_MAX-out.data)/8))return fail(error,"GM TicketQueue original array class/count/capacity/span differs");
    std::set<uintptr_t> unique;
    for(int32_t i=0;i<out.count;++i){uintptr_t member=0;if(!read(out.data,size_t(i)*8,member,error))return false;
        if(!member||!unique.insert(member).second)return fail(error,"GM TicketQueue original array contains a null or duplicate Ticket");out.members.push_back(member);}
    return true;
}

// Ticket objects have no UID. Their array index is only a local witness within
// this complete queue census, never a persistent identity or peer pointer.
template<class Read,class FindUID>bool read(Read& at,FindUID& find_uid,uintptr_t image,uintptr_t queue,uintptr_t expected_root,
    const std::vector<uintptr_t>& master_objects,Census& out,std::string& error){
    out={};Census next;next.queue=queue;uintptr_t current=0,ap=0;
    if(!at(image,0x13df360,current,error)||!at(queue,0,ap,error))return false;
    if(!queue||!expected_root||current!=queue||ap!=image+0x16fdc60||master_objects.size()>50)return fail(error,"GM actual TicketQueue singleton/class or fixed master pool differs");
    if(!span(at,image,queue+0x18,next.tickets,error))return false;
    std::map<uintptr_t,size_t> masters,tickets;std::set<uint64_t> uids;
    for(size_t i=0;i<master_objects.size();++i){Master row;row.object=master_objects[i];uintptr_t registered=0;
        if(!at(row.object,0,ap,error)||!at(row.object,0x18,row.uid,error)||!at(row.object,0x4ac,row.slot,error)||!at(row.object,0x4b0,row.state,error)||
           !at(row.object,0x4b8,row.ticket,error)||!at(row.object,0x170,row.root,error)||!at(row.object,0x178,row.region,error)||
           !at(row.object,0x100,row.orientation,error)||!at(row.object,0x110,row.position,error)||!at(row.object,0x11c,row.scale,error)||!at(row.object,0x4c0,row.timers[0],error)||!at(row.object,0x4c8,row.timers[1],error)||!at(row.object,0x4cc,row.timers[2],error)||
           !at(row.object,0x4c4,row.employed,error)||!at(row.object,0x4c5,row.active,error)||!at(row.object,0x368,row.on_map,error)||!at(row.object,0x4d0,row.evidence,error))return false;
        const bool owned_root=row.root==expected_root||(!row.root&&!row.on_map&&!row.state&&!row.ticket);
        if(ap!=image+0x16e5070||!owned_root||!row.uid||row.slot!=int32_t(i)||row.state<0||row.state>4||row.employed>1||row.active>1||row.on_map>1||row.evidence>1||
           !masters.emplace(row.object,i).second||!uids.insert(row.uid).second)return fail(error,"GM actual fixed slot/class/full UID/state/flags differ");
        if(!find_uid(row.uid,registered,error))return false;
        if(registered!=row.object)return fail(error,"GM original FindUID does not resolve its current fixed slot");
        for(const auto values:{row.position,row.timers,row.scale})for(float value:values)if(!std::isfinite(value))return fail(error,"GM original position/timer/scale is not finite");
        for(float value:row.orientation)if(!std::isfinite(value))return fail(error,"GM original orientation is not finite");
        if(!at(row.object,0x498,row.arrow,error)||!at(row.arrow,0,ap,error)||!at(row.arrow,0x1e8,row.arrow_target,error)||!at(row.arrow,0x250,row.arrow_has_target,error))return false;
        if(ap!=image+0x16fbbf0||row.arrow_has_target>1)return fail(error,"GM original FlyingArrow class/target-present flag differs");
        for(float value:row.arrow_target)if(!std::isfinite(value))return fail(error,"GM original FlyingArrow target is not finite");
        next.masters.push_back(row);
    }
    for(size_t i=0;i<next.tickets.members.size();++i){Ticket row;row.object=next.tickets.members[i];
        if(!at(row.object,0,ap,error)||!at(row.object,0x18,row.type,error)||!at(row.object,0x1c,row.position,error)||
           !at(row.object,0x50,row.toon,error)||!at(row.object,0x58,row.toon_proxy,error)||!at(row.object,0x60,row.region,error)||!at(row.object,0x68,row.master,error)||!at(row.object,0x70,row.valid,error))return false;
        if(ap!=image+0x1787fe0||row.type<0||row.type>3||row.valid>1||(row.toon&&!row.toon_proxy)||
           !tickets.emplace(row.object,i).second)return fail(error,"GM original Ticket class/type/valid/Toon link differs");
        if(row.toon_proxy){uintptr_t target=0;if(!at(row.toon_proxy,0,target,error))return false;
            if(target&&target!=row.toon)return fail(error,"GM Ticket weak Toon proxy resolves to another object");row.toon_alive=target!=0;}
        for(float value:row.position)if(!std::isfinite(value))return fail(error,"GM Ticket original position is not finite");
        next.values.push_back(row);
    }
    for(const auto& row:next.values)if(row.master){const auto found=masters.find(row.master);
        if(found==masters.end()||next.masters[found->second].ticket!=row.object)return fail(error,"GM Ticket assignment lacks exact current reciprocal master ownership");}
    for(const auto& row:next.masters)if(row.ticket){const auto found=tickets.find(row.ticket);
        if(found==tickets.end()||next.values[found->second].master!=row.object)return fail(error,"GM current master Ticket is absent or not reciprocal in its queue");}
    Span after;if(!span(at,image,queue+0x18,after,error))return false;
    if(after!=next.tickets)return fail(error,"GM TicketQueue original order/storage changed during census");
    if(!at(image,0x13df360,current,error))return false;
    if(current!=queue)return fail(error,"GM TicketQueue singleton changed during census");
    out=std::move(next);error.clear();return true;
}
}
