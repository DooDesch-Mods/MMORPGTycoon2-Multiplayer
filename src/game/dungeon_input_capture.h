#pragma once
#include "typed_action/dungeon_input_action.h"
namespace mt2mp::game::dungeon_input_capture {
struct Purchase {
    uintptr_t cursor=0,receiver=0,source=0;
    bool clear_placement=false,treasure=false;
    action_session::DungeonInputIntent intent;
};
inline bool fail(std::string& error,const char* cause){error=cause;return false;}
// Called only from the original Door, Key and Treasure Purchase call sites. The outer
// capture binds current World/Scene/Controller before retaining this value.
template<class Read,class Copy,class Text>
bool purchase(uintptr_t image,uintptr_t caller,uintptr_t type,uintptr_t position,
              Read&& at,Copy&& copy,Text&& text,Purchase& out,std::string& error){
    out={};const bool door=caller==image+0x5a34b2,key=caller==image+0x5a636a,treasure=caller==image+0x5b7f9a;
    if(!door&&!key&&!treasure)return fail(error,"Dungeon input Purchase caller is not original Door Press, Key Update or Treasure Update");
    Purchase value;uintptr_t ap=0,ghost=0;
    if(!at(image,0x13df9d0,value.cursor,error)||!at(value.cursor,0,ap,error))return false;
    if(ap!=image+0x1786dd0)return fail(error,"Dungeon input actual native Cursor class differs");
    if(!at(value.cursor,0x190,value.receiver,error)||!at(value.receiver,0,ap,error))return false;
    if(ap!=image+(door?0x173e870:treasure?0x1732980:0x173d330))return fail(error,"Dungeon input Purchase receiver is not the exact active Door/Key/Treasure behaviour");
    value.treasure=treasure;value.intent.tool=door?action_session::DungeonInputTool::Door:treasure?action_session::DungeonInputTool::Treasure:action_session::DungeonInputTool::Key;
    if(door){
        uintptr_t data=0;int32_t index=-1,count=-1,capacity=-1;uint8_t legal=0;
        if(!at(value.receiver,0x468,index,error)||!at(value.receiver,0x178,data,error)||!at(value.receiver,0x180,count,error)||
           !at(value.receiver,0x184,capacity,error)||!at(value.receiver,0x3f4,legal,error))return false;
        if(!data||index<0||count<1||index>=count||capacity<count||capacity>1000000||data>UINTPTR_MAX-size_t(capacity)*0x40||
           type!=data+size_t(index)*0x40||legal!=1)return fail(error,"Dungeon input Door selected native ListData/valid preview differs");
        value.source=value.receiver+0x388;
        if(!text(type,0,value.intent.type,error)||!text(type,0x20,value.intent.variant,error))return false;
    }else{
        uint8_t active=2,pending=2;
        if(!at(value.receiver,0x6e0,active,error)||!at(value.receiver,0x6e1,pending,error)||!at(value.receiver,0x740,ghost,error))return false;
        if(active!=0||pending!=1||!ghost||type!=value.receiver+0x168)
            return fail(error,"Dungeon input Key is not the original completed placement/pending Purchase");
        value.clear_placement=true;value.source=ghost+0x100;
        if(!text(value.receiver,0x168,value.intent.type,error)||!text(value.receiver,0x188,value.intent.variant,error))return false;
    }
    if(!at(value.source,0,value.intent.rotation,error)||!at(value.source,0x10,value.intent.position,error)||
       !at(value.source,0x1c,value.intent.scale,error))return false;
    std::array<float,3> actual{};if(!copy(position,actual.data(),sizeof actual,error))return false;
    if(actual!=value.intent.position||!action_session::monster_pose(value.intent.position,value.intent.rotation,value.intent.scale)||
       value.intent.type.empty()||value.intent.type.size()>256||value.intent.variant.size()>256||
       value.intent.type.find('\0')!=std::string::npos||value.intent.variant.find('\0')!=std::string::npos)
        return fail(error,"Dungeon input Purchase selected native resource/preview Transform/argument differs");
    uintptr_t current=0;if(!at(value.cursor,0x190,current,error))return false;
    if(current!=value.receiver)return fail(error,"Dungeon input Purchase active behaviour changed during capture");
    out=std::move(value);error.clear();return true;
}
}
