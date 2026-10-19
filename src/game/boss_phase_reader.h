#pragma once
#include "typed_action/boss_phase_action.h"
#include <algorithm>
#include <cmath>
#include <set>
#include <string>
#include <vector>
namespace mt2mp::game::boss_phase_reader {
struct LocalPhase {
    uintptr_t pointer=0,data=0;int32_t capacity=0;
    std::vector<std::array<uintptr_t,2>> elements;
    bool operator==(const LocalPhase&)const=default;
};
struct LocalType {
    uintptr_t object=0,parent=0,phase_data=0,ability_data=0;
    uint64_t uid=0;int32_t phase_capacity=0,ability_capacity=0;
    std::vector<uintptr_t> abilities;
    std::vector<LocalPhase> phases;
    bool operator==(const LocalType&)const=default;
};
template<class Read>bool array(Read& read,uintptr_t object,size_t offset,uintptr_t& data,int32_t& count,int32_t& capacity,std::string& e){
    if(!read(object,offset+8,data,e)||!read(object,offset+16,count,e)||!read(object,offset+20,capacity,e))return false;
    if(count<0||count>10000||capacity<count||(count&&!data)||data>UINTPTR_MAX-size_t(count)*8){e="Boss phase native array bounds differ offset="+std::to_string(offset);return false;}return true;
}
template<class Read,class Text>bool type(Read& read,Text& text,uintptr_t image,uintptr_t object,int32_t id,
    action_session::BossTypePhases& out,LocalType& local,std::string& e){
    out={};local={};out.id=id;local.object=object;uintptr_t ap=0;int32_t kind=-1,actual=-1;uint8_t deleted=2,edited=2,released=2;
    if(!object||!read(object,0,ap,e)||ap!=image+0x1705990||!read(object,0x18,local.uid,e)||!local.uid||
       !read(object,0x2c,kind,e)||kind!=3||!read(object,0x30,actual,e)||actual!=id||
       !read(object,0x4a0,local.parent,e)||!read(object,0x490,deleted,e)||deleted>1||
       !read(object,0x491,edited,e)||edited>1||!read(object,0x492,released,e)||released>1||
       !read(object,0x2e0,out.rename_day,e)||!text(object+0xb0,out.name,e)||!text(object+0x248,out.definition,e)){
        if(e.empty())e="Boss phase CharacterType class/full UID/ID/flags differs";return false;}
    out.deleted=deleted!=0;out.edited=edited!=0;out.released=released!=0;
    int32_t ability_count=0,phase_count=0;
    if(!array(read,object,0x128,local.ability_data,ability_count,local.ability_capacity,e)||
       !array(read,object,0x158,local.phase_data,phase_count,local.phase_capacity,e))return false;
    std::set<uintptr_t> unique_abilities,unique_phases,unique_elements;
    for(int32_t i=0;i<ability_count;++i){uintptr_t ability=0;
        if(!read(local.ability_data,size_t(i)*8,ability,e)||!ability||!unique_abilities.insert(ability).second||
           !read(ability,0,ap,e)||ap!=image+0x171c930){if(e.empty())e="Boss phase Character ability membership differs";return false;}
        std::string name;if(!text(ability+0x48,name,e))return false;out.ability_names.push_back(std::move(name));local.abilities.push_back(ability);
    }
    for(int32_t i=0;i<phase_count;++i){LocalPhase phase;action_session::BossPhaseRow row;int32_t ordinal=-1,count=0;
        if(!read(local.phase_data,size_t(i)*8,phase.pointer,e)||!phase.pointer||!unique_phases.insert(phase.pointer).second||
           !read(phase.pointer,0,ap,e)||ap!=image+0x16ddb00||!read(phase.pointer,0x5c,ordinal,e)||ordinal!=i||
           !read(phase.pointer,0x58,row.health,e)||!read(phase.pointer,0x80,row.color,e)||
           !text(phase.pointer+0x38,row.name,e)||!text(phase.pointer+0x60,row.vfx,e)||
           !array(read,phase.pointer,0x90,phase.data,count,phase.capacity,e)){
            if(e.empty())e="Boss phase exact class/order/fields differ phase="+std::to_string(i);return false;}
        for(float value:row.color)if(!std::isfinite(value)){e="Boss phase color is nonfinite";return false;}
        for(int32_t j=0;j<count;++j){uintptr_t element=0,ability=0;
            if(!read(phase.data,size_t(j)*8,element,e)||!element||!unique_elements.insert(element).second||
               !read(element,0,ap,e)||ap!=image+0x173a3a0||!read(element,0x18,ability,e)){
                if(e.empty())e="Boss phase element class/ownership differs";return false;}
            const auto found=std::find(local.abilities.begin(),local.abilities.end(),ability);
            if(found==local.abilities.end()){e="Boss phase Ability prototype is outside owning CharacterType";return false;}
            row.abilities.push_back(int32_t(found-local.abilities.begin()));phase.elements.push_back({element,ability});
        }
        out.phases.push_back(std::move(row));local.phases.push_back(std::move(phase));
    }
    uintptr_t after=0;int32_t count=0,capacity=0;
    if(!array(read,object,0x158,after,count,capacity,e)||after!=local.phase_data||count!=phase_count||capacity!=local.phase_capacity||
       !array(read,object,0x128,after,count,capacity,e)||after!=local.ability_data||count!=ability_count||capacity!=local.ability_capacity){
        if(e.empty())e="Boss phase arrays changed during complete read";return false;}
    return true;
}
inline bool phase_set_local(const LocalType& before,const LocalType& after,const action_session::BossPhaseIntent& intent){
    auto expected=before;
    if(intent.kind==action_session::BossPhaseKind::Destroy){
        if(intent.phase<0||size_t(intent.phase)>=expected.phases.size())return false;
        expected.phases.erase(expected.phases.begin()+intent.phase);
    }else if(intent.kind==action_session::BossPhaseKind::Create){
        if(size_t(intent.phase)!=before.phases.size()||after.phases.size()!=before.phases.size()+1)return false;
        const auto& added=after.phases.back();if(!added.pointer||!added.elements.empty()||added.capacity!=4||!added.data)return false;
        for(const auto& phase:before.phases)if(phase.pointer==added.pointer)return false;
        if(before.phases.size()<size_t(before.phase_capacity)){if(after.phase_data!=before.phase_data||after.phase_capacity!=before.phase_capacity)return false;}
        else{if(after.phase_capacity!=std::max(4,before.phase_capacity*2)||!after.phase_data)return false;expected.phase_data=after.phase_data;expected.phase_capacity=after.phase_capacity;}
        expected.phases.push_back(added);
    }else return false;
    return expected==after;
}
inline bool membership_local(const LocalType& before,const LocalType& after,const action_session::BossPhaseIntent& intent){
    if(intent.phase<0||size_t(intent.phase)>=before.phases.size()||after.phases.size()!=before.phases.size())return false;
    auto expected=before;auto& phase=expected.phases[size_t(intent.phase)];const auto& current=after.phases[size_t(intent.phase)];
    if(intent.kind==action_session::BossPhaseKind::RemoveAbility){
        if(intent.element<0||size_t(intent.element)>=phase.elements.size())return false;
        phase.elements.erase(phase.elements.begin()+intent.element);
    }else if(intent.kind==action_session::BossPhaseKind::AddAbility){
        if(current.elements.size()!=phase.elements.size()+intent.ability_indices.size())return false;
        if(before.phases[size_t(intent.phase)].elements.size()+intent.ability_indices.size()<=size_t(phase.capacity)){
            if(current.data!=phase.data||current.capacity!=phase.capacity)return false;
        }else{if(current.capacity<phase.capacity||current.capacity<int32_t(current.elements.size())||!current.data)return false;phase.data=current.data;phase.capacity=current.capacity;}
        for(size_t i=0;i<intent.ability_indices.size();++i){const auto index=intent.ability_indices[i];if(index<0||size_t(index)>=before.abilities.size())return false;
            const auto& added=current.elements[phase.elements.size()];if(!added[0]||added[1]!=before.abilities[size_t(index)])return false;
            for(const auto& old:before.phases)for(const auto& element:old.elements)if(element[0]==added[0])return false;
            phase.elements.push_back(added);
        }
    }else return false;
    return expected==after;
}
inline bool reordered_local(const LocalType& before,const LocalType& after,int32_t source,int32_t boundary){
    if(source<0||size_t(source)>=before.phases.size()||boundary<0||size_t(boundary)>before.phases.size())return false;
    auto expected=before;auto phase=expected.phases[size_t(source)];expected.phases.erase(expected.phases.begin()+source);
    const auto destination=boundary>source?boundary-1:boundary;expected.phases.insert(expected.phases.begin()+destination,std::move(phase));
    return expected==after;
}
}
