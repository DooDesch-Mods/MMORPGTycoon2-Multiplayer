#pragma once
#include "group_reader.h"

namespace mt2mp::game::ordered_clearance_group {
struct Member {
    group_reader::Handle handle;
    action_session::GroupMember relative;
    action_session::GroupProp prop;
    bool operator==(const Member&)const=default;
};
struct State {
    group_reader::Handle handle;
    uintptr_t root=0,region=0;
    action_session::GroupRow row;
    std::vector<Member> members;
    bool operator==(const State&)const=default;
};
// Read one actual parent and its reciprocal members. Its first read occurs
// before the original Scenery destructor removes a member from this list.
template<class Memory,class Bind,class Region,class Find,class Key>
bool read(Memory& memory,uintptr_t image,uintptr_t root,uintptr_t group,
          Bind&& bind,Region&& region,Find&& find,Key&& key,State& out,std::string& error){
    namespace q=npc_quest_targets;State next;uintptr_t parent=0,found=0;q::Bounds members;
    next.handle.object=group;
    if(!q::typed(memory,image,group,0x1749de0,error)||!q::at(memory,group,0x18,next.handle.uid,error)||
       !q::at(memory,group,0x170,next.root,error)||!q::at(memory,group,0x178,next.region,error)||
       !q::at(memory,group,0x180,parent,error)||!find(next.handle.uid,found,error)||
       !region(next.region,next.row.region,error)||!group_reader::pose(memory,group,0x100,next.row.pose,error)||
       !q::typed(memory,image,group+0x210,0x16ea660,error)||!q::bounds(memory,group+0x210,100000,members,error))return false;
    if(!next.handle.uid||found!=group||next.root!=root||parent)
        return q::fail(error,"Affected Group full UID/root/parent differs");
    std::set<uintptr_t> records,objects;std::set<uint64_t> uids;std::set<std::string> keys;
    for(int32_t i=0;i<members.count;++i){uintptr_t record=0,actual_root=0,actual_parent=0;Member member;
        if(!q::at(memory,members.data,size_t(i)*8,record,error)||!q::typed(memory,image,record,0x16fbc40,error)||
           !q::at(memory,record,0x18,member.handle.object,error)||!group_reader::pose(memory,record,0x64,member.relative.relative,error)||
           !q::typed(memory,image,member.handle.object,0x16c1390,error)||!q::at(memory,member.handle.object,0x18,member.handle.uid,error)||
           !q::at(memory,member.handle.object,0x170,actual_root,error)||!q::at(memory,member.handle.object,0x180,actual_parent,error)||
           !group_reader::pose(memory,member.handle.object,0x100,member.prop.pose,error)||
           !bind(member.handle.object,member.handle.uid,member.prop.key,member.prop.region,error))return false;
        if(!member.handle.uid||actual_root!=root||actual_parent!=group||member.prop.key.empty()||
           !records.insert(record).second||!objects.insert(member.handle.object).second||
           !uids.insert(member.handle.uid).second||!keys.insert(member.prop.key).second)
            return q::fail(error,"Affected Group reciprocal member/full UID/canonical key repeats or differs");
        member.relative.key=member.prop.key;next.row.members.push_back(member.relative);next.members.push_back(std::move(member));
    }
    if(!q::unchanged(memory,group+0x210,100000,members,error)||
       !key(next.handle.uid,action_session::group_key(next.row),next.row.key,error)||next.row.key.empty())
        return q::fail(error,error.empty()?"Affected Group retained canonical key is absent":error);
    for(auto& member:next.members)member.prop.parent=next.row.key;
    out=std::move(next);return true;
}
inline bool removed_members(const State& before,const State& after,const std::set<uint64_t>& removed,std::string& error){
    auto expected=before;
    std::erase_if(expected.members,[&](const Member& member){return removed.contains(member.handle.uid);});
    expected.row.members.clear();for(const auto& member:expected.members)expected.row.members.push_back(member.relative);
    return expected==after||npc_quest_targets::fail(error,"Affected Group does not retain its exact owner/pose and original ordered surviving members");
}
}
