#pragma once
#include "scenery_stroke_history.h"
#include "terrain_history_payload.h"

namespace mt2mp::game::terrain_history_make {
using Saved=scenery_make_history::Saved;
// A new inverse captures the current live Transform. Keep its complete bytes
// for later execution while binding its authored pose to the approved current row.
template<class Memory,class Matrix>bool capture(Memory& memory,uintptr_t image,uintptr_t node,
    const Saved& current,const terrain_scenery_diagnostic::Row& row,Matrix&& matrix,Saved& out,std::string& e){
    namespace q=npc_quest_targets;Saved stored=current;
    if(!q::at(memory,node,0xb8,stored.uid,e)||!q::at(memory,node,0xc0,stored.type,e)||
       !q::at(memory,node,0xc8,stored.root,e)||!q::at(memory,node,0xd0,stored.transform,e)||
       !q::at(memory,node,0x13c,stored.height,e)||!q::at(memory,node,0x140,stored.automatic,e))return false;
    terrain_history_payload::Report report;report.add(current,stored,row,row,true,matrix);
    if(!scenery_stroke_history::same_inputs(current,stored)||report.failed){
        e="Terrain history current Make payload differs: "+report.json().dump();return false;}
    if(!scenery_make_history::make(memory,image,node,stored,e)){
        e+=" current_make_payload="+report.json().dump();return false;}
    out=stored;return true;
}
}
