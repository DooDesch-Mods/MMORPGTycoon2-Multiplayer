#pragma once
#include "group_reader.h"
namespace mt2mp::game::group_inverse {
struct Target {uint64_t group_uid=0;uintptr_t root=0,region=0;std::array<uint32_t,10> pose{};std::vector<group_reader::Handle> members;};
template<class Memory,class Find>bool read(Memory& m,uintptr_t image,uintptr_t graph,bool grouping,
                                         const std::vector<Target>& expected,Find&& find,std::string& e){
    namespace q=group_reader::q;q::Bounds operations;
    if(!q::typed(m,image,graph+0x18,0x16e2dc0,e)||!q::bounds(m,graph+0x18,100000,operations,e))return false;
    if(operations.count!=int32_t(expected.size()))return q::fail(e,"Group original inverse operation count differs");
    for(size_t i=0;i<expected.size();++i){uintptr_t node=0;uint64_t uid=0;const auto& target=expected[i];
        if(!q::at(m,operations.data,i*8,node,e)||!q::typed(m,image,node,grouping?0x1723370:0x16ff380,e)||!q::at(m,node,0xb8,uid,e))return false;
        if(uid!=target.group_uid)return q::fail(e,"Group original inverse full Group UID differs");
        if(!grouping){uintptr_t root=0,region=0;std::array<uint32_t,10> pose;
            if(!q::at(m,node,0xc0,root,e)||!q::at(m,node,0xc8,region,e)||!q::at(m,node,0xf0,pose,e))return false;
            if(root!=target.root||region!=target.region||pose!=target.pose)return q::fail(e,"Ungroup original inverse saved root/Region/pose differs");}
        q::Bounds members;const size_t offset=grouping?0xc0:0x160;
        if(!q::typed(m,image,node+offset,0x1749370,e)||!q::bounds(m,node+offset,100000,members,e))return false;
        if(members.count!=int32_t(target.members.size()))return q::fail(e,"Group original inverse ordered member count differs");
        for(size_t j=0;j<target.members.size();++j){uint32_t stored=0;uintptr_t actual=0;const auto& wanted=target.members[j];
            if(!q::at(m,members.data,j*4,stored,e)||!find(uint64_t(stored),actual,e))return false;
            // The original inverse array stores int32 IDs. Verify that its
            // actual FindUID result still is this complete live identity.
            if(uint64_t(stored)!=wanted.uid||actual!=wanted.object)return q::fail(e,"Group original inverse Scenery full UID/order differs");}
        if(!q::unchanged(m,node+offset,100000,members,e))return false;
    }
    return q::unchanged(m,graph+0x18,100000,operations,e);
}
}
