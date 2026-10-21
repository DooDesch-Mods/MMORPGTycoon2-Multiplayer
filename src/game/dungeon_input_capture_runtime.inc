namespace dungeon_input {
namespace {
bool purchase_entry(const dungeon_input_calls::Text& type,const void* point)try{
    const auto caller=uintptr_t(_ReturnAddress());
    if(replaying||!net::session_wire::connected()||(caller!=state.image+0x5a34b2&&caller!=state.image+0x5a636a&&caller!=state.image+0x5b7f9a))
        return original_purchase(type,point);
    std::string error;Memory memory;Pending value;value.snapshot=net::current_snapshot_id();
    if(stroke||!idle(value.binding,value.snapshot,error)||!code(memory,error))return reject(error.empty()?"Dungeon input cannot replace an active Paint collection":error);
    auto at=[&](uintptr_t base,size_t offset,auto& output,std::string& cause){return field(memory,base,offset,output,cause);};
    auto copy=[&](uintptr_t address,void* output,size_t bytes,std::string& cause){return memory.copy(address,output,bytes,cause);};
    auto text=[&](uintptr_t base,size_t offset,std::string& output,std::string& cause){return npc_quest_records::read_string(memory,base,offset,"Dungeon input selected resource",output,cause);};
    dungeon_input_capture::Purchase actual;
    if(!dungeon_input_capture::purchase(state.image,caller,uintptr_t(&type),uintptr_t(point),at,copy,text,actual,error))return reject(error);
    value.intent=actual.intent;if(!capture_scene(value.intent,error))return reject(error);
    const dungeon_input_calls::Text selected_type{value.intent.type.data(),uint64_t(value.intent.type.size())};
    if(!original_result(dungeon_input_calls::cost(state.image,selected_type,value.intent.cost),"captured GetTypeCost",error)||!finish_capture(memory,value,error))return reject(error);
    if(actual.clear_placement){
        Sample before,after;uint8_t active=2,completed=2;
        if(!sample(memory,value.binding,value.snapshot,value.intent,before,error))return reject(error);
        if(!original_result(dungeon_input_action_calls::placement_clear(state.image,actual.receiver,actual.treasure),"UI-only completed Key/Treasure placement Clear",error)||
           !field(memory,actual.receiver,0x6e0,active,error)||!field(memory,actual.receiver,0x6e1,completed,error)||active||completed||
           !sample(memory,value.binding,value.snapshot,value.intent,after,error)||before.value!=after.value||
           before.source.palette.dungeon.dungeon.dungeon.money!=after.source.palette.dungeon.dungeon.dungeon.money){
            fault=true;return reject(error.empty()?"Dungeon input original Key/Treasure Clear changed gameplay or did not clear its completed UI placement":error);}
    }
    pending=std::move(value);++captures;
    // False returns before the original native Pay/AddGizmo branch. Key's
    // genuine PlacementLogic::Clear has already completed its local UI state.
    return false;
}catch(const std::exception& exception){fault=true;return reject(std::string("Dungeon input Purchase capture boundary: ")+exception.what());}
catch(...){fault=true;return reject("Dungeon input Purchase capture non-standard boundary exception");}

bool current_tool(Memory& memory,uintptr_t receiver,uintptr_t expected,uintptr_t supplied_cursor,uintptr_t& cursor,std::string& error){
    uintptr_t ap=0,active=0;
    if(!field(memory,state.image,0x13df9d0,cursor,error)||!field(memory,cursor,0,ap,error))return false;
    if((supplied_cursor&&supplied_cursor!=cursor)||ap!=state.image+0x1786dd0)return fail(error,"Dungeon input actual native Cursor owner differs");
    if(!field(memory,cursor,0x190,active,error)||!field(memory,receiver,0,ap,error))return false;
    return active==receiver&&ap==state.image+expected||fail(error,"Dungeon input receiver is not its exact active native behaviour");
}
void monster_release_entry(void* receiver,void* supplied_cursor,const void* point)try{
    if(replaying||!net::session_wire::connected()){original_monster_release(receiver,supplied_cursor,point);return;}
    Memory memory;std::string error;uintptr_t cursor=0,preview=0,definition=0;uint8_t warp=2,pressed=2;
    if(uintptr_t(_ReturnAddress())!=state.image+0x587e6c||!code(memory,error)||
       !current_tool(memory,uintptr_t(receiver),0x1741060,uintptr_t(supplied_cursor),cursor,error)||
       !field(memory,uintptr_t(receiver),0x338,preview,error)||!field(memory,uintptr_t(receiver),0x3d9,warp,error)){
        reject(error.empty()?"Dungeon input Monster Release lacks original Cursor dispatcher ownership":error);return;}
    if(warp==1||!preview){original_monster_release(receiver,supplied_cursor,point);return;}
    if(warp!=0){reject("Dungeon input Monster original Warp UI flag outside boolean domain");return;}
    Pending value;value.snapshot=net::current_snapshot_id();value.intent.tool=a::DungeonInputTool::Monster;
    if(stroke||!idle(value.binding,value.snapshot,error)||!field(memory,uintptr_t(receiver),0x3da,pressed,error)||pressed!=1||
       !field(memory,uintptr_t(receiver),0x340,definition,error)||!definition||!field(memory,definition,0x30,value.intent.character_type,error)||
       !field(memory,uintptr_t(receiver),0x35c,value.intent.rank,error)||!field(memory,uintptr_t(receiver),0x360,value.intent.rotation,error)||
       !field(memory,uintptr_t(receiver),0x370,value.intent.position,error)||!field(memory,uintptr_t(receiver),0x37c,value.intent.scale,error)||
       !capture_scene(value.intent,error)){
        reject(error.empty()?"Dungeon input Monster Release lacks pressed/current original selection":error);return;}
    dungeon_input_action_calls::LevelSelection level;
    if(!original_result(dungeon_input_action_calls::monster_level(state.image,uintptr_t(receiver),level),"selected native Monster level",error)){
        reject(error);return;}
    value.intent.level=level.range[0];if(!finish_capture(memory,value,error)){reject(error);return;}
    Sample before,after;bool cleared=false;
    if(!sample(memory,value.binding,value.snapshot,value.intent,before,error)||
       !original_result(dungeon_input_action_calls::complete_monster_release(state.image,cursor,uintptr_t(receiver),cleared),"exact local Monster Release UI tail",error)||!cleared||
       !sample(memory,value.binding,value.snapshot,value.intent,after,error)||before.value!=after.value||
       before.source.palette.dungeon.dungeon.dungeon.money!=after.source.palette.dungeon.dungeon.dungeon.money){
        fault=true;reject(error.empty()?"Dungeon input Monster UI completion changed native gameplay or failed its readback":error);return;}
    pending=std::move(value);++captures;
}catch(const std::exception& exception){fault=true;reject(std::string("Dungeon input Monster Release capture boundary: ")+exception.what());}
catch(...){fault=true;reject("Dungeon input Monster Release capture non-standard boundary exception");}

bool paint_owner(Memory& memory,uintptr_t receiver,uintptr_t& canvas,std::string& mode,std::string& error){
    uintptr_t cursor=0,current_stroke=0,graph=0;
    if(!current_tool(memory,receiver,0x173f1d0,0,cursor,error)||!field(memory,receiver,0x178,canvas,error)||
       !field(memory,receiver,0x930,current_stroke,error)||!field(memory,receiver,0x948,graph,error)||
       !npc_quest_records::read_string(memory,receiver,0x150,"Dungeon input original Paint mode",mode,error))return false;
    if(!canvas||current_stroke||graph)return fail(error,"Dungeon input normal Paint capture has an existing native stroke/edit graph");
    return mode=="paint"||mode=="erase"||fail(error,"Dungeon input original Paint mode is not paint/erase");
}
void paint_stamp_entry(void* receiver,const void* shape)try{
    if(replaying||!net::session_wire::connected()){original_paint_stamp(receiver,shape);return;}
    std::string error,mode;Memory memory;uintptr_t canvas=0;
    Pending value;value.snapshot=net::current_snapshot_id();value.intent.tool=a::DungeonInputTool::Paint;
    if(!idle(value.binding,value.snapshot,error)||!code(memory,error)||!paint_owner(memory,uintptr_t(receiver),canvas,mode,error)||!capture_scene(value.intent,error)){
        reject(error);return;}
    value.intent.erase=mode=="erase";a::TerrainShape clipped;
    if(!ui_terrain_runtime::poly::read(memory,uintptr_t(shape),clipped,error)){reject(error);return;}
    if(clipped.empty())return;
    uintptr_t selected_fill=0;
    if(!value.intent.erase){uintptr_t window=0;bool selected=false;
        if(!field(memory,uintptr_t(receiver),0x5d0,window,error)||!window||
           !original_result(dungeon_input_action_calls::fill_selection(state.image,window,selected,selected_fill),"FillEditor selection",error)){
            reject(error.empty()?"Dungeon input original FillEditor is absent":error);return;}
        if(!selected)return;
        if(!stroke){dungeon_cells::ValueMemory view(memory);dungeon_fill::State fill;
            if(!dungeon_fill::read(state.image,view,selected_fill,fill,value.intent.fill,error)||!fill.present){reject(error.empty()?"Dungeon input original selected Fill is absent":error);return;}}
    }
    if(stroke){
        if(stroke->receiver!=uintptr_t(receiver)||stroke->canvas!=canvas||stroke->fill!=selected_fill||stroke->pending.binding!=value.binding||
           stroke->pending.snapshot!=value.snapshot||stroke->pending.intent.scene!=value.intent.scene||stroke->pending.intent.erase!=value.intent.erase){
            fault=true;reject("Dungeon input Paint receiver/current scene/selection changed during one original stroke");return;}
        value.intent.fill=stroke->pending.intent.fill;value.intent.shapes=stroke->pending.intent.shapes;
    }
    value.intent.shapes.push_back(std::move(clipped));value.intent.before_sha256=std::string(64,'0');
    if(!a::valid_dungeon_input(value.intent)){fault=true;reject("Dungeon input clipped native Paint shape exceeds fixed tool/coordinate/resource bounds");return;}
    a::DungeonPaletteIntent target;target.key=value.intent.key;dungeon_palette::Sample owner;
    if(!dungeon_palette::sample(memory,value.binding,value.snapshot,target,owner,error)||canvas!=owner.dungeon.dungeon.dungeon.local.dungeon+0x500){
        fault=true;reject(error.empty()?"Dungeon input Paint Canvas is not owned by the exact current canonical Dungeon":error);return;}
    stroke=Stroke{uintptr_t(receiver),canvas,selected_fill,std::move(value)};
}catch(const std::exception& exception){fault=true;reject(std::string("Dungeon input Paint capture boundary: ")+exception.what());}
catch(...){fault=true;reject("Dungeon input Paint capture non-standard boundary exception");}

void paint_finish_entry(void* receiver,const void* shape)try{
    if(replaying||!net::session_wire::connected()){original_paint_finish(receiver,shape);return;}
    Memory memory;std::string error,mode;uintptr_t canvas=0;
    if(!code(memory,error)||!paint_owner(memory,uintptr_t(receiver),canvas,mode,error)){
        fault=true;reject(error);return;}
    if(!stroke){original_paint_finish(receiver,shape);return;}
    auto saved=*stroke;stroke.reset();a::Binding binding;
    if(!idle(binding,saved.pending.snapshot,error)||binding!=saved.pending.binding||saved.receiver!=uintptr_t(receiver)||canvas!=saved.canvas||
       saved.pending.intent.erase!=(mode=="erase")){
        fault=true;reject(error.empty()?"Dungeon input PaintingFinished no longer owns its exact captured stroke":error);return;}
    if(!saved.pending.intent.erase){dungeon_cells::ValueMemory view(memory);dungeon_fill::State fill;json records;uintptr_t window=0,current_fill=0;bool selected=false;
        if(!field(memory,uintptr_t(receiver),0x5d0,window,error)||!window||
           !original_result(dungeon_input_action_calls::fill_selection(state.image,window,selected,current_fill),"current FillEditor selection",error)||
           !selected||current_fill!=saved.fill||!dungeon_fill::read(state.image,view,current_fill,fill,records,error)||!fill.present||records!=saved.pending.intent.fill){
            fault=true;reject(error.empty()?"Dungeon input original selected Fill changed before PaintingFinished":error);return;}}
    if(!finish_capture(memory,saved.pending,error)){fault=true;reject(error);return;}
    Sample before,after;if(!sample(memory,binding,saved.pending.snapshot,saved.pending.intent,before,error)){fault=true;reject(error);return;}
    // AddPaint was suppressed and the two original effect pointers are null.
    // The original finish now runs once solely to clear its local UI counters.
    original_paint_finish(receiver,shape);
    if(!sample(memory,binding,saved.pending.snapshot,saved.pending.intent,after,error)||before.value!=after.value||
       before.source.palette.dungeon.dungeon.dungeon.money!=after.source.palette.dungeon.dungeon.dungeon.money){
        fault=true;reject(error.empty()?"Dungeon input original UI-only PaintingFinished changed gameplay":error);return;}
    pending=std::move(saved.pending);++captures;
}catch(const std::exception& exception){fault=true;reject(std::string("Dungeon input PaintingFinished capture boundary: ")+exception.what());}
catch(...){fault=true;reject("Dungeon input PaintingFinished capture non-standard boundary exception");}
}
}
