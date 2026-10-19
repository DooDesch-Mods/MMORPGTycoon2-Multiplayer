#pragma once
#include "npc_quest_targets.h"
#include "typed_action/group_action.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <map>
namespace mt2mp::game::group_reader {
namespace q=npc_quest_targets;
namespace a=action_session;
struct Handle {uintptr_t object=0;uint64_t uid=0;bool operator==(const Handle&)const=default;};
struct Local {
    uintptr_t manager=0,selection=0;
    a::GroupState state;
    std::map<std::string,Handle> handles;
    std::vector<uintptr_t> raw,filtered;
    bool operator==(const Local&)const=default;
};
template<class Memory>bool ui(Memory& m,uintptr_t image,uintptr_t selection_owner,uintptr_t single,std::string& e){
    uintptr_t window=0,command=0;uint8_t visible=0;
    if(single){uintptr_t focused=0,handler=0,target=0;
        if(!q::at(m,selection_owner,0x80,focused,e)||!q::at(m,selection_owner,0xb8,handler,e)||
           !q::at(m,handler,0x10,window,e)||!q::typed(m,image,window,0x1719fd0,e)||!q::at(m,window,0x4e8,target,e))return false;
        if(focused!=single||target!=single)return q::fail(e,"Group InfoWindow selected owner differs");
        if(!q::at(m,image+0x1719fd0,0x168,command,e)||command!=image+0x718c80)return q::fail(e,"Group InfoWindow command differs");
    }else if(!q::at(m,selection_owner,0xc0,window,e)||!q::typed(m,image,window,0x171b260,e)||
             !q::at(m,image+0x171b260,0x168,command,e)||command!=image+0x74bc00)return q::fail(e,e.empty()?"Group MultiInfoWindow owner/command differs":e);
    if(!q::at(m,window,0x45f,visible,e))return false;
    return visible==1||q::fail(e,"Group normal source InfoWindow is not visible");
}
template<class Memory>bool pose(Memory& m,uintptr_t object,size_t offset,std::array<uint32_t,10>& out,std::string& e){
    if(!q::at(m,object,offset,out,e))return false;
    for(auto bits:out)if(!std::isfinite(std::bit_cast<float>(bits)))return q::fail(e,"Group transform contains nonfinite native value");
    return true;
}
template<class Memory>bool selection(Memory& m,uintptr_t image,uintptr_t owner,std::vector<uintptr_t>& raw,std::vector<uintptr_t>& filtered,std::string& e){
    uintptr_t singleton=0;uint8_t setting=0,queued=0;
    if(!q::at(m,image,0x13df630,singleton,e)||singleton!=owner||!q::typed(m,image,owner,0x17205d0,e)||
       !q::at(m,owner,0xd1,setting,e)||!q::at(m,owner,0xd3,queued,e))return q::fail(e,e.empty()?"Group exact SelectionManager differs":e);
    if(setting||queued)return q::fail(e,"Group SelectionManager is setting/queued");
    auto read=[&](size_t offset,std::vector<uintptr_t>& out){q::Bounds span;
        if(!q::typed(m,image,owner+offset,0x1748c50,e)||!q::bounds(m,owner+offset,100000,span,e))return false;
        out.clear();std::set<uintptr_t> unique;
        for(int32_t i=0;i<span.count;++i){uintptr_t p=0;if(!q::at(m,span.data,size_t(i)*8,p,e))return false;
            if(!p||!unique.insert(p).second)return q::fail(e,"Group Selection null/duplicate member");out.push_back(p);}
        return q::unchanged(m,owner+offset,100000,span,e);};
    return read(0x20,raw)&&read(0x38,filtered);
}
// BindScenery validates the existing canonical saved/action key, full UID,
// current world and original Scenery manager before returning its owner Region.
template<class Memory,class BindScenery,class Owner,class Find,class BindGroup>
bool read(Memory& m,uintptr_t image,uintptr_t root,uintptr_t selection_owner,const std::vector<uintptr_t>& extra,
          BindScenery&& bind,Owner&& owner,Find&& find,BindGroup&& group_key,Local& out,std::string& e){
    Local result;result.selection=selection_owner;q::Bounds groups;
    if(!q::at(m,image,0x13df3e0,result.manager,e)||!q::typed(m,image,result.manager,0x1700890,e)||
       !q::typed(m,image,result.manager+0x18,0x16f0060,e)||!q::bounds(m,result.manager+0x18,100000,groups,e))return false;
    std::set<uintptr_t> objects,member_objects;std::set<uint64_t> uids;
    auto scenery=[&](uintptr_t p,uintptr_t expected_parent,a::GroupProp& prop){
        uintptr_t actual_root=0,parent=0;uint64_t uid=0;
        if(!q::typed(m,image,p,0x16c1390,e)||!q::at(m,p,0x18,uid,e)||!q::at(m,p,0x170,actual_root,e)||
           !q::at(m,p,0x180,parent,e)||!pose(m,p,0x100,prop.pose,e)||!bind(p,uid,prop.key,prop.region,e))return false;
        if(!uid||actual_root!=root||parent!=expected_parent||prop.key.empty())return q::fail(e,"Group Scenery full UID/root/reciprocal parent differs");
        const auto [it,added]=result.handles.emplace(prop.key,Handle{p,uid});
        if(!added&&it->second!=Handle{p,uid})return q::fail(e,"Group canonical Scenery key aliases another full UID");
        if(added&&!uids.insert(uid).second)return q::fail(e,"Group/Scenery complete local full UID repeats");
        return true;};
    for(int32_t i=0;i<groups.count;++i){uintptr_t p=0,actual_root=0,region=0,parent=0,found=0;uint64_t uid=0;a::GroupRow row;q::Bounds members;
        if(!q::at(m,groups.data,size_t(i)*8,p,e)||!q::typed(m,image,p,0x1749de0,e)||!q::at(m,p,0x18,uid,e)||
           !q::at(m,p,0x170,actual_root,e)||!q::at(m,p,0x178,region,e)||!q::at(m,p,0x180,parent,e)||
           !owner(region,row.region,e)||!find(uid,found,e)||!pose(m,p,0x100,row.pose,e)||
           !q::typed(m,image,p+0x210,0x16ea660,e)||!q::bounds(m,p+0x210,100000,members,e))return false;
        if(!uid||found!=p||actual_root!=root||parent||!objects.insert(p).second||!uids.insert(uid).second)
            return q::fail(e,"Group current manager/full UID/root ownership differs");
        std::vector<a::GroupProp> props;std::set<uintptr_t> member_records;
        for(int32_t j=0;j<members.count;++j){uintptr_t record=0,prop=0;a::GroupMember member;a::GroupProp value;
            if(!q::at(m,members.data,size_t(j)*8,record,e)||!q::typed(m,image,record,0x16fbc40,e)||
               !q::at(m,record,0x18,prop,e)||!pose(m,record,0x64,member.relative,e)||!scenery(prop,p,value))return false;
            if(!member_records.insert(record).second||!member_objects.insert(prop).second)
                return q::fail(e,"Group member record/Scenery ownership repeats");
            member.key=value.key;row.members.push_back(std::move(member));props.push_back(std::move(value));
        }
        if(!q::unchanged(m,p+0x210,100000,members,e))return false;
        const auto derived=a::group_key(row);
        if(!group_key(p,uid,derived,row.key,e)||row.key.empty())return q::fail(e,e.empty()?"Group current canonical full UID binding is absent":e);
        if(!result.handles.emplace(row.key,Handle{p,uid}).second)return q::fail(e,"Group canonical full UID identity repeats");
        for(auto& prop:props){prop.parent=row.key;result.state.props.push_back(std::move(prop));}
        result.state.groups.push_back(std::move(row));
    }
    for(auto p:extra){if(objects.contains(p)||member_objects.contains(p))continue;a::GroupProp prop;
        if(!scenery(p,0,prop))return false;member_objects.insert(p);result.state.props.push_back(std::move(prop));}
    if(!q::unchanged(m,result.manager+0x18,100000,groups,e)||!selection(m,image,selection_owner,result.raw,result.filtered,e))return false;
    std::sort(result.state.props.begin(),result.state.props.end(),[](const auto& x,const auto& y){return x.key<y.key;});
    out=std::move(result);return true;
}
template<class Memory,class BindScenery,class Owner,class Find>
bool read(Memory& m,uintptr_t image,uintptr_t root,uintptr_t selection_owner,const std::vector<uintptr_t>& extra,
          BindScenery&& bind,Owner&& owner,Find&& find,Local& out,std::string& e){
    auto derive=[](uintptr_t,uint64_t,const std::string& source,std::string& key,std::string& error){
        if(source.empty())return q::fail(error,"Group has no retained canonical identity or original member");key=source;return true;};
    return read(m,image,root,selection_owner,extra,bind,owner,find,derive,out,e);
}
}
