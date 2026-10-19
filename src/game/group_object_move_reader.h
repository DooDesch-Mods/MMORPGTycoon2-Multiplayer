#pragma once
#include "group_reader.h"
#include "group_object_transition.h"
#include <cstring>

namespace mt2mp::game::group_object_move_reader {
struct Capture {
    uintptr_t behaviour=0,records=0,blueprint=0,ghost=0,target=0;
    uint64_t uid=0;int32_t capacity=0;
    action_session::GroupObjectIntent intent;
    bool operator==(const Capture&)const=default;
};
template<class Memory>bool target(Memory& m,uintptr_t image,uintptr_t behaviour,bool& group,std::string& e){
    group=false;int32_t count=0;uintptr_t records=0,object=0,ap=0;
    if(!npc_quest_targets::at(m,behaviour,0x468,count,e))return false;
    if(count!=1)return true;
    if(!npc_quest_targets::at(m,behaviour,0x460,records,e)||!records||
       !npc_quest_targets::at(m,records,0x10,object,e)||!object||!npc_quest_targets::at(m,object,0,ap,e))return false;
    group=ap==image+0x1749de0;return true;
}
template<class Memory,class Bind>bool read(Memory& m,uintptr_t image,uintptr_t root,uintptr_t behaviour,Bind&& bind,size_t budget,Capture& out,std::string& e){
    namespace q=npc_quest_targets;out={};out.behaviour=behaviour;
    uint8_t pending=0,duplicate=0;int32_t count=0;uintptr_t ghost_blueprint=0,shard=0,can_move=0,floating=0;
    if(!q::typed(m,image,behaviour,0x172f1f0,e)||!q::at(m,behaviour,0x401,pending,e)||pending!=1||
       !q::at(m,behaviour,0x499,duplicate,e)||duplicate||!q::typed(m,image,behaviour+0x458,0x1747300,e)||
       !q::at(m,behaviour,0x460,out.records,e)||!q::at(m,behaviour,0x468,count,e)||!q::at(m,behaviour,0x46c,out.capacity,e))
        return q::fail(e,e.empty()?"Group Move original pending nonduplicate behaviour differs":e);
    if(count!=1||out.capacity<count||!out.records||out.records>UINTPTR_MAX-24)return q::fail(e,"Group Move requires one complete original Group record");
    if(!q::at(m,out.records,0,out.blueprint,e)||!q::at(m,out.records,8,out.ghost,e)||!q::at(m,out.records,16,out.target,e)||
       !q::typed(m,image,out.target,0x1749de0,e)||!q::at(m,out.target,0x18,out.uid,e)||!out.uid||out.ghost==out.target||
       !q::typed(m,image,out.ghost,0x1705230,e)||!q::at(m,out.ghost,0x210,ghost_blueprint,e)||ghost_blueprint!=out.blueprint||
       !q::typed(m,image,out.blueprint,0x16dd870,e)||!q::at(m,out.blueprint,8,shard,e)||shard!=root||
       !q::at(m,image+0x1749de0,0xd0,can_move,e)||can_move!=image+0xe47100||
       !q::at(m,image+0x1749de0,0x100,floating,e)||floating!=image+0x4ceaa0)
        return q::fail(e,e.empty()?"Group Move original Group/Ghost/Blueprint/current root differs":e);
    // The binding supplies the complete ordered Scenery membership. Its native
    // floating predicate is true, so the original Group predicate is true too.
    if(!bind(out.target,out.uid,out.intent,e)||!group_reader::pose(m,out.ghost,0x100,out.intent.after,e))return false;
    auto key=[](const std::string& value){return group_object_key::valid("group:"+value);};
    if(!group_object_transition::valid(true,out.intent,key,budget))return q::fail(e,"Group Move original complete intent is unchanged or invalid");
    uintptr_t records=0;int32_t final_count=0,capacity=0;
    if(!q::at(m,behaviour,0x460,records,e)||!q::at(m,behaviour,0x468,final_count,e)||!q::at(m,behaviour,0x46c,capacity,e))return false;
    return (records==out.records&&final_count==1&&capacity==out.capacity)||q::fail(e,"Group Move original record header changed during capture");
}
template<class Memory>bool cleaned(Memory& m,uintptr_t image,const Capture& value,std::string& e){
    namespace q=npc_quest_targets;uintptr_t records=0,options=0,cursor=0,current=0,first=0,second=0;int32_t count=-1,capacity=0;
    if(!q::typed(m,image,value.behaviour,0x172f1f0,e)||!q::at(m,value.behaviour,0x460,records,e)||!q::at(m,value.behaviour,0x468,count,e)||
       !q::at(m,value.behaviour,0x46c,capacity,e)||!q::at(m,value.behaviour,0x490,options,e)||!q::at(m,image,0x13df9d0,cursor,e)||
       !q::at(m,cursor,0x190,current,e)||!q::at(m,value.records,0,first,e)||!q::at(m,value.records,8,second,e))return false;
    return (records==value.records&&capacity==value.capacity&&!count&&!options&&current!=value.behaviour&&!first&&!second)||
        q::fail(e,"Group Move original owned Blueprint/Ghost and cursor cleanup is incomplete");
}
inline bool transform(const std::array<uint32_t,10>& pose,std::array<unsigned char,0x6c>& out,std::string& e){
    out={};if(!duplicate_transition::pose(pose)){e="Group Move native transform is invalid";return false;}
    std::memcpy(out.data(),pose.data(),sizeof pose);out[0x28]=1;return true;
}
}
