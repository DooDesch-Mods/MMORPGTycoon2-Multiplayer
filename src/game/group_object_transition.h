#pragma once
#include "typed_action/group_object_action.h"
#include "group_object_key.h"
#include "group_transition.h"
#include "duplicate_transition.h"

namespace mt2mp::game::group_object_transition {
namespace a=action_session;
template<class Key>bool valid(bool moving,const a::GroupObjectIntent& intent,Key&& key,size_t budget){
    if(!group_object_key::valid(intent.key)||!duplicate_transition::pose(intent.before)||intent.members.size()>budget)return false;
    if(moving?!duplicate_transition::pose(intent.after)||intent.after==intent.before:intent.after!=std::array<uint32_t,10>{})return false;
    std::set<std::string> members;
    for(const auto& member:intent.members)if(!key(member)||!members.insert(member).second)return false;
    return true;
}
template<class Key>bool valid(const a::GroupObjectState& state,const std::string& target,Key&& key,size_t budget){
    if(!group_object_key::valid(target)||!group_transition::valid(state.ownership,key)||
       !state.records.created.empty()||state.records.history||state.records.sources.size()>budget||
       !duplicate_transition::sha(state.records.untouched_sha)||state.records.random!=state.ownership.random)return false;
    const auto group=std::find_if(state.ownership.groups.begin(),state.ownership.groups.end(),[&](const auto& row){return row.key==target;});
    if(group==state.ownership.groups.end())return state.records.sources.empty();
    if(state.records.sources.size()!=group->members.size()+1)return false;
    std::map<std::string,const a::DuplicateObject*> records;
    for(const auto& row:state.records.sources)if(!duplicate_transition::sha(row.record_sha)||!duplicate_transition::pose(row.pose)||!records.emplace(row.key,&row).second)return false;
    const auto parent=records.find(target);
    if(parent==records.end()||parent->second->family!=a::DuplicateFamily::Group||!parent->second->parent.empty()||
       parent->second->region!=group->region||parent->second->pose!=group->pose||parent->second->members.size()!=group->members.size())return false;
    for(size_t index=0;index<group->members.size();++index){const auto& member=group->members[index];const auto found=records.find(member.key);
        const auto prop=std::find_if(state.ownership.props.begin(),state.ownership.props.end(),[&](const auto& row){return row.key==member.key;});
        if(found==records.end()||parent->second->members[index]!=member.key||found->second->family!=a::DuplicateFamily::Scenery||
           found->second->parent!=target||!found->second->members.empty()||prop==state.ownership.props.end()||
           found->second->region!=prop->region||found->second->pose!=prop->pose)return false;
    }
    return true;
}
template<class Key>bool effect(bool moving,const a::GroupObjectIntent& intent,const a::GroupObjectState& before,
                               const a::GroupObjectState& after,Key&& key,size_t budget,std::string& e,bool restoring=false){
    auto fail=[&](const char* cause){e=cause;return false;};
    if(!valid(moving,intent,key,budget)||!valid(before,intent.key,key,budget)||!valid(after,intent.key,key,budget))return fail("Group complete intent/state is invalid");
    const auto source=std::find_if(before.ownership.groups.begin(),before.ownership.groups.end(),[&](const auto& row){return row.key==intent.key;});
    const auto destination=std::find_if(after.ownership.groups.begin(),after.ownership.groups.end(),[&](const auto& row){return row.key==intent.key;});
    if(source==before.ownership.groups.end()||source->pose!=intent.before||source->members.size()!=intent.members.size())return fail("Group original source pose/member count differs");
    for(size_t index=0;index<intent.members.size();++index)if(source->members[index].key!=intent.members[index])return fail("Group original ordered member identity differs");
    if(moving){if(destination==after.ownership.groups.end()||destination->pose!=intent.after||destination->members!=source->members)return fail("Group original moved pose/relative members differ");}
    else if(destination!=after.ownership.groups.end())return fail("Group original Destroy retained its Group");
    if(before.records.untouched_sha!=after.records.untouched_sha||before.records.release!=after.records.release||(!restoring&&before.records.random!=after.records.random))
        return fail("Group original transaction changed an unrelated Record, Release counter or RNG");
    auto groups=[](a::GroupState state,const std::string& target){std::erase_if(state.groups,[&](const auto& row){return row.key==target;});return state.groups;};
    if(groups(before.ownership,intent.key)!=groups(after.ownership,intent.key))return fail("Group original transaction changed another Group");
    auto props=[&](const a::GroupState& state){std::vector<a::GroupProp> out;for(const auto& prop:state.props)
        if(std::find(intent.members.begin(),intent.members.end(),prop.key)==intent.members.end())out.push_back(prop);return out;};
    if(props(before.ownership)!=props(after.ownership))return fail("Group original transaction changed an unrelated Scenery owner");
    if(!moving)for(const auto& prop:after.ownership.props)
        if(std::find(intent.members.begin(),intent.members.end(),prop.key)!=intent.members.end())return fail("Group original Destroy retained a member");
    return true;
}
}
