namespace dungeon_cells {
namespace {
namespace calls=dungeon_cells_calls;
struct Sample {dungeon_palette::Sample palette;dungeon_scene_identity_runtime::Census scenes;dungeon_scene_records::Witness records;
    qr::Witness quests;quest_list::References references;a::DungeonPaletteState value;
    std::vector<std::pair<std::string,identity::Entry>> monsters;};
struct MonsterMemory {Memory& source;bool read(uintptr_t p,void* out,size_t bytes,std::string& error){return source.copy(p,out,bytes,error);}};
bool monster_members(Memory& memory,const dungeon_scene_identity_runtime::Census& scenes,
                     std::vector<std::pair<std::string,identity::Entry>>& out,bool allow_missing,std::string& error,const identity::Registry* prepared=nullptr){
    out.clear();std::vector<std::pair<std::string,identity::Entry>> aliases;
    const auto root=uintptr_t(scenes.world.map)+0x1c0;
    const auto& registry=prepared?*prepared:identity::registry;
    if(!registry.enumerate(scenes.world.uuid,scenes.world.world_epoch,scenes.world.generation,root,aliases,error))return false;
    MonsterMemory reader{memory};std::set<std::string> seen;
    for(const auto& scene:scenes.pools){monster_saved_identity::Pool pool;
        const auto unlinked=[](uintptr_t,int32_t& link,std::string&){link=0;return true;};
        if(!monster_saved_identity::observe(reader,state.image,scene.managers[2],unlinked,pool,error))return false;
        for(const auto& row:pool.records){const auto found=std::find_if(aliases.begin(),aliases.end(),[&](const auto& candidate){const auto& entry=candidate.second;
                return !entry.retired&&entry.scope.manager==pool.manager&&entry.scope.region==scene.scene.owner.region&&entry.pointer==row.pointer&&entry.uid==row.uid;});
            if(found==aliases.end()||!seen.insert(found->first).second)return fail(error,"Dungeon cell current Monster lacks a unique confirmed complete scene/full-UID alias");
            out.push_back(*found);
        }
        if(!allow_missing)for(const auto& [key,entry]:aliases)if(!entry.retired&&entry.scope.manager==pool.manager&&!seen.contains(key))
            return fail(error,"Dungeon cell current Monster census omitted a live confirmed scene alias");
    }return true;
}
bool retire_monsters(Memory& memory,const Sample& before,std::string& error){
    std::vector<std::pair<std::string,identity::Entry>> actual;if(!monster_members(memory,before.scenes,actual,true,error))return false;
    for(const auto& row:actual)if(std::find(before.monsters.begin(),before.monsters.end(),row)==before.monsters.end())return fail(error,"Dungeon cell Close cascade introduced or replaced a Monster alias");
    host::LookupApi api;if(!host::resolve_lookup(memory.process,state.image,exe_sha,api,error))return false;
    auto next=identity::registry;
    for(const auto& [key,entry]:before.monsters)if(std::find(actual.begin(),actual.end(),std::pair{key,entry})==actual.end()){
        uintptr_t found=0;const auto seh=lookup(api.find,entry.uid,found);
        if(seh||found)return fail(error,"Dungeon cell queued Monster retirement is incomplete SEH="+std::to_string(seh)+" registered="+std::to_string(bool(found)));
        if(!next.retire(key,entry,error))return false;
    }
    identity::registry=std::move(next);return true;
}
bool add_owner(std::map<uintptr_t,std::string>& owners,uintptr_t pointer,const std::string& key,std::string& error){
    if(!pointer||key.empty())return fail(error,"Dungeon cell complete external owner is empty");
    const auto [found,inserted]=owners.emplace(pointer,key);return inserted||found->second==key||fail(error,"Dungeon cell complete external owner aliases two keys");
}
bool external_owners(Memory& memory,const a::Binding& binding,const std::string& snapshot,const dungeon_palette::Sample& sample,
                     std::map<uintptr_t,std::string>& out,std::string& error){
    out.clear();const auto& world=sample.dungeon.dungeon.dungeon.world;
    if(!add_owner(out,world.root,"root_shard",error))return false;
    auto at=[&](uintptr_t base,size_t offset,auto& value,std::string& e){return field(memory,base,offset,value,e);};
    const auto map=uintptr_t(world.origin.map);uintptr_t data=0;int32_t count=0;
    if(!at(map+0x230,8,data,error)||!at(map+0x230,0x10,count,error)||count<1||count>4096)return fail(error,error.empty()?"Dungeon cell complete saved Region census differs":error);
    for(int32_t i=0;i<count;++i){uintptr_t region=0,selected=0;int32_t id=-1;
        if(!at(data,size_t(i)*8,region,error)||!at(region,0x834,id,error)||
           !region_saved_identity::resolve(state.image,map,world.root,id,region,selected,at,error)||!add_owner(out,region,"region:"+std::to_string(id),error))return false;
    }
    host::LookupApi api;if(!host::resolve_lookup(memory.process,state.image,exe_sha,api,error))return false;
    json bindings;if(!remove_diagnostics::bindings(bindings,error))return false;
    for(const auto& row:bindings.at("bindings")){
        if(row.at("revoked").get<bool>())continue;
        if(row.at("unknown").get<bool>()||row.at("snapshot_id")!=snapshot)return fail(error,"Dungeon cell shared prop registry is unknown or belongs to another snapshot");
        const auto uid=row.at("local_uid").get<uint64_t>();uintptr_t pointer=0;
        const auto seh=lookup(api.find,uid,pointer);
        if(seh||!pointer)return fail(error,"Dungeon cell shared prop FindUID failed SEH="+std::to_string(seh));
        uint64_t actual=0;if(!at(pointer,0x18,actual,error)||actual!=uid||!add_owner(out,pointer,row.at("record_key").get<std::string>(),error))return false;
    }
    a::Context context;std::vector<action_runtime::StateOwner> owners;
    if(!a::selected_session().current(context,error)||context.snapshot_uuid!=snapshot||!action_runtime::state_owners(context,binding,owners,error))return false;
    for(const auto& owner:owners){if(!add_owner(out,owner.npc,owner.key,error))return false;
        for(const auto& [slot,pointer]:owner.quests)if(!add_owner(out,pointer,owner.key+"/quest/"+std::to_string(slot),error))return false;}
    const auto native_state=uintptr_t(world.origin.state);uintptr_t manager=0;character_definition_identity::Census definitions;
    if(!at(native_state,0x168,manager,error)||!character_definition_identity::read(at,state.image,native_state,manager,definitions,error))return false;
    for(const auto& item:definitions.definitions)if(!item.deleted&&!add_owner(out,item.object,item.key,error))return false;
    return true;
}
bool sample(Memory& memory,const a::Binding& binding,const std::string& snapshot,const a::DungeonPaletteIntent& intent,
            Sample& out,std::string& error,const qr::Witness* previous=nullptr){
    out={};if(!dungeon_palette::sample(memory,binding,snapshot,intent,out.palette,error))return false;
    std::map<uintptr_t,std::string> owners;if(!external_owners(memory,binding,snapshot,out.palette,owners,error))return false;
    const auto external=[&](uintptr_t pointer,std::string& key,std::string& e){const auto found=owners.find(pointer);
        if(found==owners.end())return fail(e,"Dungeon cell original Record external reference has no current canonical owner");key=found->second;return true;};
    if(!dungeon_scene_identity_runtime::read_records(external,out.scenes,out.records,error)||!monster_members(memory,out.scenes,out.monsters,false,error)||
       !read_remove_quests(memory,binding,snapshot,0,{},out.quests,error,previous)||
       !quest_list::references(memory,binding,snapshot,out.quests,out.references,error))return false;
    out.value=out.palette.value;out.value.cells.clear();out.value.stroke.scenes_sha=out.records.sha;
    const auto dungeon=out.records.dungeons.find(intent.key);
    if(dungeon==out.records.dungeons.end())return fail(error,"Dungeon cell original complete Dungeon record is absent");
    out.value.stroke.dungeon_sha=dungeon->second;std::string quests;
    if(!quest_digest(out.quests,quests,error)||!quest_bridge::sha256(json{{"quests",quests},{"active",out.references.rows}}.dump(),out.value.stroke.quests_sha,error))return false;
    for(const auto index:intent.stroke.cells){if(index<0||size_t(index)>=out.palette.value.cells.size())return fail(error,"Dungeon cell requested index is outside the current original layout");
        out.value.stroke.cells.push_back({index,out.palette.value.cells[size_t(index)]});}
    dungeon_palette::Sample after;if(!dungeon_palette::sample(memory,binding,snapshot,intent,after,error)||
       !dungeon_palette::same_local(out.palette,after)||out.palette.value!=after.value)
        return fail(error,error.empty()?"Dungeon cell complete Record/Quest sampling changed current owner or layout":error);
    return true;
}
bool static_digest(const dungeon_palette::Sample& sample,const a::DungeonCellStroke& stroke,std::string& hash,std::string& error){
    json cells=json::array();for(const auto index:stroke.cells){if(index<0||size_t(index)>=sample.value.cells.size())return fail(error,"Dungeon cell stroke index exceeds original layout");
        const auto& cell=sample.value.cells[size_t(index)];cells.push_back({{"index",index},{"open",cell.open},{"theme",cell.theme},{"colors",cell.colors}});}
    return quest_bridge::sha256(json{{"palette",sample.value.palette},{"defaults",sample.value.defaults},{"cells",cells}}.dump(),hash,error);
}
bool theme_colors(Memory& memory,const dungeon_palette::Sample& sample,const a::DungeonCellStroke& stroke,std::array<uint32_t,4>& packed,std::string& error){
    packed={};if(stroke.mode==a::DungeonCellMode::Close)return true;
    if(sample.value.palette.size()<4)return fail(error,"Dungeon cell original palette has fewer than four colors");
    std::copy_n(sample.value.palette.begin(),4,packed.begin());uintptr_t manager=0,theme=0,ap=0,data=0;int32_t count=0,capacity=0;
    if(!field(memory,state.image,0x13df700,manager,error)||!manager)return fail(error,error.empty()?"Dungeon cell ThemeManager singleton is absent":error);
    const calls::Text key{stroke.theme.data(),uint64_t(stroke.theme.size())};const auto seh=calls::theme_lookup(state.image,manager,key,theme);
    if(seh||!theme)return fail(error,"Dungeon cell original Theme lookup failed SEH="+std::to_string(seh));
    if(!field(memory,theme,0,ap,error)||ap!=state.image+0x16ff4d0||!field(memory,theme,0x80,data,error)||
       !field(memory,theme,0x88,count,error)||!field(memory,theme,0x8c,capacity,error)||count<0||capacity<count||capacity>100000||(count&&!data))return fail(error,error.empty()?"Dungeon cell original Theme color array differs":error);
    if(!sample.value.defaults||!count)return true;
    if(count<4)return fail(error,"Dungeon cell selected Theme has fewer than four original default colors");
    std::array<std::array<float,4>,4> colors{};for(size_t i=0;i<4;++i){if(!memory.copy(data+i*16,colors[i].data(),16,error))return false;
        for(const auto channel:colors[i])if(!std::isfinite(channel))return fail(error,"Dungeon cell original Theme default color is not finite");}
    return dungeon_palette::pack(colors,packed,error);
}
}
}
