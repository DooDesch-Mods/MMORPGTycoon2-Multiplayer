#pragma once
#include "move_kiss/move_kiss.h"
#include <cmath>
#include <set>
namespace mt2mp::selection_move_reader {
struct Selection {
    move_kiss::Origin origin;
    uintptr_t behaviour=0,records=0;
    int32_t capacity=0;
    std::vector<move_kiss::Intent> rows;
    bool operator==(const Selection&)const=default;
};
template<class Height>bool read(move_kiss::Memory& memory,move_kiss::Services& services,uintptr_t image,uintptr_t behaviour,
                                const move_kiss::Origin& origin,Height& height,Selection& out,std::string& error){
    out={};auto fail=[&](const char* cause){error=cause;return false;};
    auto at=[&](uintptr_t base,size_t offset,auto& value){return base&&base<=UINTPTR_MAX-offset&&memory.read(base+offset,&value,sizeof value,error);};
    uintptr_t ap=0;uint8_t pending=0,duplicate=0;int32_t count=0;
    if(!at(behaviour,0,ap)||ap!=image+0x172f1f0||!at(behaviour,0x401,pending)||pending!=1||
       !at(behaviour,0x499,duplicate)||duplicate||!at(behaviour,0x458,ap)||ap!=image+0x1747300||
       !at(behaviour,0x460,out.records)||!at(behaviour,0x468,count)||!at(behaviour,0x46c,out.capacity))
        return error.empty()?fail("Selection Move original behaviour/pending/nonduplicate array differs"):false;
    if(count<2||size_t(count)>action_session::max_payload||out.capacity<count||!out.records||out.records>UINTPTR_MAX-size_t(count)*24)
        return fail("Selection Move record span exceeds current transport/array bounds");
    out.origin=origin;out.behaviour=behaviour;std::set<uintptr_t> targets,ghosts;std::set<uint64_t> uids;std::set<std::string> keys;
    for(int32_t i=0;i<count;++i){move_kiss::Intent row;row.origin=origin;row.behaviour=behaviour;row.record=out.records+size_t(i)*24;
        uintptr_t blueprint=0,ghost_blueprint=0;
        if(!at(row.record,0,blueprint)||!at(row.record,8,row.ghost)||!at(row.record,16,row.target)||
           !at(row.target,0x18,row.local_uid)||!at(row.ghost,0,ap)||ap!=image+0x1705230||
           !at(row.ghost,0x210,ghost_blueprint)||!blueprint||blueprint!=ghost_blueprint)
            return error.empty()?fail("Selection Move record/Ghost/Blueprint ownership differs"):false;
        if(!row.local_uid||row.ghost==row.target||!targets.insert(row.target).second||!ghosts.insert(row.ghost).second||!uids.insert(row.local_uid).second)
            return fail("Selection Move duplicate local target/Ghost/full UID");
        if(!services.bind_target(origin,row.ghost,row.target,row.local_uid,row.before,row.object_key,row.family,error))return false;
        if(row.family!=move_kiss::Family::Prop)return fail("Selection Move current family requires its complete multi-target state binding");
        if(row.object_key.empty()||!keys.insert(row.object_key).second)return fail("Selection Move canonical target key duplicated/absent");
        row.after=row.before;
        if(!at(row.ghost,0x100,row.after.rotation)||!at(row.ghost,0x110,row.after.x)||!at(row.ghost,0x114,row.after.y)||
           !at(row.ghost,0x118,row.after.z)||!at(row.ghost,0x11c,row.after.scale)||!height(row,blueprint,error))return false;
        std::array<unsigned char,0x6c> pose;
        if(!move_kiss::transform(row.before,pose,error)||!move_kiss::transform(row.after,pose,error))return false;
        out.rows.push_back(std::move(row));
    }
    uintptr_t records=0;int32_t final_count=0,capacity=0;
    if(!at(behaviour,0x460,records)||!at(behaviour,0x468,final_count)||!at(behaviour,0x46c,capacity))return false;
    return (records==out.records&&final_count==count&&capacity==out.capacity)||fail("Selection Move record header changed during read");
}
inline bool cleaned(move_kiss::Memory& memory,uintptr_t image,const Selection& value,std::string& error){
    auto at=[&](uintptr_t base,size_t offset,auto& out){return base&&base<=UINTPTR_MAX-offset&&memory.read(base+offset,&out,sizeof out,error);};
    uintptr_t ap=0,records=0,options=0,cursor=0,current=0;int32_t count=-1,capacity=0;
    if(!at(value.behaviour,0,ap)||!at(value.behaviour,0x460,records)||!at(value.behaviour,0x468,count)||
       !at(value.behaviour,0x46c,capacity)||!at(value.behaviour,0x490,options)||!at(image,0x13df9d0,cursor)||!at(cursor,0x190,current))return false;
    if(ap!=image+0x172f1f0||records!=value.records||capacity!=value.capacity||count||options||current==value.behaviour){error="Selection Move original Deactivate/header cleanup differs";return false;}
    for(const auto& row:value.rows){uintptr_t first=0,second=0;
        if(!at(row.record,0,first)||!at(row.record,8,second))return false;
        if(first||second){error="Selection Move original owned record cleanup incomplete key="+row.object_key;return false;}}
    return true;
}
}
