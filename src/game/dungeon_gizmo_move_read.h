#pragma once
#include "dungeon_gizmo_control.h"
#include "npc_quest_targets.h"
namespace mt2mp::game::dungeon_gizmo_move_read {
struct CursorRow{uintptr_t record=0,target=0,ghost=0,blueprint=0;uint64_t uid=0;bool operator==(const CursorRow&)const=default;};
struct Capture{uintptr_t behaviour=0,data=0;int32_t capacity=0;bool duplicate=false;
    std::vector<CursorRow> rows;dungeon_gizmo_control::Intent intent;bool operator==(const Capture&)const=default;};
template<class Memory,class Owner,class Point>
bool read(Memory& memory,uintptr_t image,uintptr_t behaviour,Owner&& owner,Point&& point,Capture& out,std::string& error){
    namespace q=npc_quest_targets;out={};Capture next;next.behaviour=behaviour;uint8_t pending=2,duplicate=2;q::Bounds records,charged;
    if(!q::typed(memory,image,behaviour,0x172f1f0,error)||!q::at(memory,behaviour,0x401,pending,error)||pending!=1||
       !q::at(memory,behaviour,0x499,duplicate,error)||duplicate>1||!q::typed(memory,image,behaviour+0x458,0x1747300,error)||
       !q::bounds(memory,behaviour+0x458,100000,records,error))return q::fail(error,error.empty()?"Gizmo Move current original record header differs":error);
    if(!records.count)return q::fail(error,"Gizmo Move original record array is empty");
    if(duplicate&&(!q::typed(memory,image,behaviour+0x478,0x1748c50,error)||!q::bounds(memory,behaviour+0x478,100000,charged,error)||
       charged.count!=records.count))return q::fail(error,error.empty()?"Gizmo Duplicate original charged array differs":error);
    next.data=records.data;next.capacity=records.capacity;next.duplicate=duplicate!=0;
    next.intent.command=duplicate?dungeon_gizmo_control::Command::Duplicate:dungeon_gizmo_control::Command::Move;
    std::set<uintptr_t> targets,ghosts;std::set<uint64_t> uids;
    for(int32_t i=0;i<records.count;++i){CursorRow row;row.record=records.data+size_t(i)*24;uintptr_t ghost_blueprint=0,charged_target=0;
        if(!q::at(memory,row.record,0,row.blueprint,error)||!q::at(memory,row.record,8,row.ghost,error)||!q::at(memory,row.record,16,row.target,error)||
           !q::at(memory,row.target,0x18,row.uid,error)||
           !q::typed(memory,image,row.ghost,0x1705230,error)||!q::at(memory,row.ghost,0x210,ghost_blueprint,error))return false;
        if(!row.blueprint||row.blueprint!=ghost_blueprint||row.ghost==row.target||!row.uid||!targets.insert(row.target).second||
           !ghosts.insert(row.ghost).second||!uids.insert(row.uid).second)return q::fail(error,"Gizmo Move repeated target/Ghost/full UID or Blueprint owner differs");
        if(duplicate&&(!q::at(memory,charged.data,size_t(i)*8,charged_target,error)||charged_target!=row.target))
            return q::fail(error,error.empty()?"Gizmo Duplicate original charged order differs":error);
        uintptr_t target_ap=0;if(!q::at(memory,row.target,0,target_ap,error)||
           (target_ap!=image+0x1749ab0&&target_ap!=image+0x16c1390))return q::fail(error,error.empty()?"Gizmo Move target is outside actual Gizmo/Scenery classes":error);
        dungeon_gizmo_control::Row value;
        if(!owner(row.target,row.uid,value,error)||bool(value.scenery)!=(target_ap==image+0x16c1390))return q::fail(error,error.empty()?"Gizmo Move canonical class differs from current target":error);
        if(!q::at(memory,row.ghost,0x100,value.after,error))return false;
        next.intent.rows.push_back(std::move(value));next.rows.push_back(row);
    }
    if(!point(next.intent.cash_position,error)||!q::unchanged(memory,behaviour+0x458,100000,records,error)||
       (duplicate&&!q::unchanged(memory,behaviour+0x478,100000,charged,error)))return false;
    if(!dungeon_gizmo_control::valid(next.intent))return q::fail(error,"Gizmo Move complete original intent is invalid or has no movement");
    out=std::move(next);error.clear();return true;
}
template<class Memory>bool cleaned(Memory& memory,uintptr_t image,const Capture& value,std::string& error){
    namespace q=npc_quest_targets;uintptr_t records=0,options=0,cursor=0,current=0;int32_t count=-1,capacity=-1;
    if(!q::typed(memory,image,value.behaviour,0x172f1f0,error)||!q::at(memory,value.behaviour,0x460,records,error)||
       !q::at(memory,value.behaviour,0x468,count,error)||!q::at(memory,value.behaviour,0x46c,capacity,error)||
       !q::at(memory,value.behaviour,0x490,options,error)||!q::at(memory,image,0x13df9d0,cursor,error)||!q::at(memory,cursor,0x190,current,error))return false;
    if(records!=value.data||capacity!=value.capacity||count||options||current==value.behaviour)
        return q::fail(error,"Gizmo Move original Deactivate/header cleanup differs");
    for(const auto& row:value.rows){uintptr_t blueprint=1,ghost=1;
        if(!q::at(memory,row.record,0,blueprint,error)||!q::at(memory,row.record,8,ghost,error))return false;
        if(blueprint||ghost)return q::fail(error,"Gizmo Move original owned Blueprint/Ghost cleanup is incomplete");}
    return true;
}
}
