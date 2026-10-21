#pragma once
#include "npc_quest_targets.h"
#include <algorithm>

namespace mt2mp::game::world_zone_serialization {
namespace q=npc_quest_targets;
struct Member {
    uintptr_t object=0;uint64_t uid=0;int32_t type=-1,link=0;size_t child=0;
    bool operator==(const Member&)const=default;
};
struct Pool {
    uintptr_t manager=0;
    std::array<std::vector<Member>,4> arrays;
    bool operator==(const Pool&)const=default;
};
template<class Memory,class Links>
bool read(Memory& memory,uintptr_t image,uintptr_t manager,Links& links,Pool& out,std::string& error){
    out={};Pool next;next.manager=manager;
    if(!q::typed(memory,image,manager,0x16fe170,error))return false;
    std::set<uintptr_t> pointers;std::set<uint64_t> uids;std::set<int32_t> ids;
    for(size_t type=0;type<4;++type){const auto array=manager+0x18+type*0x30;q::Bounds before;
        if(!q::typed(memory,image,array,0x172e1e0,error)||!q::bounds(memory,array,4096,before,error))return false;
        for(int32_t child=0;child<before.count;++child){Member row;row.child=size_t(child);
            if(!q::at(memory,before.data,size_t(child)*8,row.object,error)||!q::typed(memory,image,row.object,0x1745ce0,error)||
               !q::at(memory,row.object,0x18,row.uid,error)||!q::at(memory,row.object,0x24c,row.type,error)||!links(row.object,row.link,error))return false;
            if(!row.uid||row.type!=int32_t(type)||row.link<0||!pointers.insert(row.object).second||!uids.insert(row.uid).second||
               (row.link&&!ids.insert(row.link).second))return q::fail(error,"World Zone serialized type/full UID/link repeats or differs");
            next.arrays[type].push_back(row);
        }
        if(!q::unchanged(memory,array,4096,before,error))return false;
        for(size_t child=0;child<next.arrays[type].size();++child){uintptr_t object=0;uint64_t uid=0;int32_t actual_type=-1;
            const auto& row=next.arrays[type][child];
            if(!q::at(memory,before.data,child*8,object,error)||!q::at(memory,row.object,0x18,uid,error)||!q::at(memory,row.object,0x24c,actual_type,error))return false;
            if(object!=row.object||uid!=row.uid||actual_type!=row.type)return q::fail(error,"World Zone serialization array/member changed during read");
        }
    }
    out=std::move(next);error.clear();return true;
}
struct Load {
    int32_t active=-1;
    std::array<int32_t,4> declared{-1,-1,-1,-1};
    std::array<std::map<uintptr_t,int32_t>,4> registrations;
    bool children(const std::string& label,int32_t before,int32_t count,std::string& error){
        static const std::array<std::string,4> labels{"None","Monster","Puzzle","Story"};
        const auto found=std::find(labels.begin(),labels.end(),label);
        if(found==labels.end()||before!=count||count<0||count>4096)return q::fail(error,"World Zone original child label/count differs");
        const auto type=int32_t(found-labels.begin());
        if(type!=active+1||declared[size_t(type)]!=-1||
           (active>=0&&registrations[size_t(active)].size()!=size_t(declared[size_t(active)])))
            return q::fail(error,"World Zone original enum order or preceding registrations differ");
        declared[size_t(type)]=count;active=type;return true;
    }
    bool registered(int32_t type,uintptr_t object,int32_t link,std::string& error){
        if(type!=active||type<0||type>3||!object||link<0)return q::fail(error,"World Zone registration outside its original enum child");
        auto& rows=registrations[size_t(type)];
        if(rows.size()>=size_t(declared[size_t(type)])||!rows.emplace(object,link).second)
            return q::fail(error,"World Zone registration exceeds declared children or repeats");
        return true;
    }
    template<class Memory>
    bool finish(Memory& memory,uintptr_t image,uintptr_t manager,Pool& out,std::string& error)const{
        if(active!=3)return q::fail(error,"World Zone original load omitted an enum group");
        for(size_t type=0;type<4;++type)if(declared[type]<0||registrations[type].size()!=size_t(declared[type]))
            return q::fail(error,"World Zone original load omitted a declared registration");
        auto links=[&](uintptr_t object,int32_t& link,std::string& cause){
            for(const auto& rows:registrations){const auto found=rows.find(object);if(found!=rows.end()){link=found->second;return true;}}
            return q::fail(cause,"World Zone current member lacks original load registration");};
        Pool next;if(!read(memory,image,manager,links,next,error))return false;
        for(size_t type=0;type<4;++type){if(next.arrays[type].size()!=size_t(declared[type]))return q::fail(error,"World Zone loaded array size differs from original count");
            for(const auto& row:next.arrays[type])if(!registrations[type].contains(row.object))return q::fail(error,"World Zone loaded member migrated from its original enum group");}
        out=std::move(next);return true;
    }
};
}
